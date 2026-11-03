#include "KEYU.h"
#include "key.h"
#include "Run_mode_panel.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_gpio.h"
#include "general_panel.h"
#include "scale_panel.h"
#include "cursor_panel.h"
#include "task.h"     // vTaskDelay / configASSERT 依赖的任务 API
#include "semphr.h"   // xSemaphoreCreateCounting / xSemaphoreTake
key_Handle KeyU;

/* 对外暴露的计数信号量：扫描任务负责 Give，专职任务负责 Take。
 * 为什么用计数型(4,0)而不是二值信号量：
 *   - 计数上限 4 表示"最多缓存 4 次按下事件"，连按 4 次以内都不会丢；
 *   - 第 5 次按下时 xSemaphoreGive 直接返回失败，扫描任务不会被阻塞——
 *     按键扫描任务每 10ms 要跑一遍 6 个键，绝不能为了一个键停下来等。
 * 之前的代码把这一行注释掉、并把 KeyU.xSem 置 NULL，
 * 导致 key.c 里 `xSemaphoreGive(key->xSem)` 那一支永远不会被走到，
 * 也就是校准报告里说的"KEYU 信号量路不可达"。
 */
SemaphoreHandle_t xKeyUSem = NULL;

GPIO_PinState keyUReadPin(void){
    return HAL_GPIO_ReadPin(KEYU, KEYUPIN);
}
void keyUCallback(void* args){
    (void)args;
     vadjust_general_panel(1);
    vadjust_RUN_mode_panel(1);
    vadjust_SCALE_panel(1);
    AdjustCursorPanel(1);
    // TODO: KeyD 按下要执行的动作
}
void keyUScan(void){
    vkeyOneSca(&KeyU);
}

/* KEYU 的专职执行任务：只做两件事——等信号量、拿到后执行一次回调。
 *
 * 原文这里被整段注释掉，于是"信号量被 Give 了却没人 Take"，
 * 触发链断在消费者一侧。本任务就是补上的那一环。
 *
 * 为什么值得单独开一个任务（而不是复用 vKeyWorkerTask 的队列）：
 *   1) KEYU 的回调会改参数并让信号发生器重算波形（vWaveGenSetFrequency），
 *      属于"慢回调"；如果和 KEY1~KEY4、KEYD 共用同一个队列消费者，
 *      一次慢回调会把后面所有按键事件堵在队列里（队列容量只有 10）；
 *   2) 用独立信号量 + 独立任务，等于把"慢事件"从公共队列上摘下来，
 *      公共队列的响应时间不再受 KEYU 影响——这正是 docs/FreeRTOS面试复盘.md
 *      里"慢回调改用信号量通知专职任务"的设计意图；
 *   3) 代价很小：等不到信号量时本任务处于阻塞挂起态，不占 CPU。
 */
void vKeyUTask(void *pv){
    (void)pv;
    for(;;){
        if (xKeyUSem == NULL) {
            /* 信号量是在 keyUInit()（运行于扫描任务上下文）里创建的，
             * 正常调度顺序下扫描任务优先运行，本任务第一次执行时信号量已就绪；
             * 这里保留 NULL 判断，是为了覆盖"信号量创建失败/调度顺序变化"的情况，
             * 避免对一个空句柄调用 xSemaphoreTake 直接触发 configASSERT。
             */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (xSemaphoreTake(xKeyUSem, portMAX_DELAY) == pdPASS) {
            keyUCallback(NULL);   // 收到一次"按下"，执行一次动作
        }
    }
}

void keyUInit(void){
    /* 只在第一次初始化时创建，避免重复调用 keyUInit 泄漏信号量 */
    if (xKeyUSem == NULL) {
        xKeyUSem = xSemaphoreCreateCounting(4, 0);
        configASSERT(xKeyUSem != NULL);
    }

    /* 路由改成 ROUTE_SEM：vkeyOneSca() 检测到按下后走
     * `xSemaphoreGive(key->xSem)` 分支，而不是往公共按键队列里塞 job_t。
     * 这样 KEYU 的触发链变成：扫描任务 Give → vKeyUTask Take → keyUCallback。
     */
    KeyU.rounte = ROUTE_SEM;
    KeyU.xSem   = xKeyUSem;   // 与 keyUScan() 使用同一个句柄

    KeyU.Init.arg = NULL;
    /* 走信号量后不再由队列消费者调用回调，这里保留回调指针是为了：
     * ① 让 key_Handle 的结构保持统一（其它键都填了）；
     * ② 以后若要切回队列路由（rounte = ROUTE_QUEUE）不需要再改这里。
     */
    KeyU.Init.ClickCallback = keyUCallback;
    KeyU.Init.keyReadPin    = keyUReadPin;
    keyInit(&KeyU);
}
