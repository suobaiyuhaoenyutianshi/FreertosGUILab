#include "KEYU.h"
#include "key.h"
#include "Run_mode_panel.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_gpio.h"
#include "general_panel.h"
#include "scale_panel.h"
#include "cursor_panel.h"
key_Handle KeyU;

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

/* KEYU 和 KEYD 的回调会改 UI 参数，并通过 vWaveGenSet*() 重算最多 1000 点波表、
 * 重启 DAC 和 TIM5；在无 FPU 的 Cortex-M3 上是毫秒级操作，还可能卡在
 * xTimerChangePeriod 的阻塞等待里。所以它们不走公共队列，而是把 job_t 投到
 * xSlowKeyQue，由唯一的 KEYSLOW 任务（keyTask.c）按顺序执行：
 *   1) 公共队列（KEY1~KEY4）的响应时间不再受慢回调影响；
 *   2) 只有一个执行者，不会出现"上/下两个回调并发改同一批全局状态和 DAC/TIM5"。
 * 任务在 main.c 里创建，栈深度见 keyTask.h 的 TASK_STACK_KEY_SLOW。
 */
void keyUInit(void){
    KeyU.id = 5;
    KeyU.rounte = ROUTE_SLOW;
    KeyU.xSem   = NULL;   // 走慢队列，不需要信号量
    KeyU.Init.arg = NULL;
#ifdef BM_BAREMETAL
    KeyU.Init.ClickCallback = NULL;
#else
    KeyU.Init.ClickCallback = keyUCallback;
#endif
    KeyU.Init.keyReadPin    = keyUReadPin;
    keyInit(&KeyU);
}
