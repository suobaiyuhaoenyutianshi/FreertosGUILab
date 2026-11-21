#include "bmcore.h"
#include "dmm.h"
#include "keyTask.h"
#include <sys/_intsup.h>

// 域 A 的确定性节拍：1 ms tick，软件分频出 10 ms 按键扫描和 10 Hz DMM
// 只做短活：没有阻塞、没有 malloc、没有锁；长活仍在域 B 的 LCD/UART 任务里
#define BMCORE_DIV_10MS 10

static TaskHandle_t xBmCoreTask = NULL;

// 调试用：可在调试器里看 BmCore 是否在跑、10 Hz 是否在走
static volatile uint32_t g_bmCoreTicks = 0;
static volatile uint32_t g_bmCoreDmmTicks = 0;

// @作用：域 A 的 1 ms 核心步进
// @说明：RTOS 宿主由 BmCore 任务每 1 ms 调一次；裸机宿主可以在 main 的 1 ms
//        节拍里直接调同一个函数。函数内部不阻塞、不 malloc、不拿锁。
void vBmCoreTick(void)
{
    static uint32_t ulDiv10ms = 0;

    g_bmCoreTicks++;

    if(++ulDiv10ms >= BMCORE_DIV_10MS)
    {
        ulDiv10ms = 0;

        vKeyScanStep(); // 10 ms 按键扫描：只入队，回调仍在 KEYWORK/KEYSLOW
        vDMMProcess();  // 消费 ADC ISR 的 raw 快照，发布 DMM 邮箱/事件位

        g_bmCoreDmmTicks++;
    }
}

static void vBmCoreTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWake = xTaskGetTickCount();

    for(;;)
    {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1));
        vBmCoreTick(); // 只保留“RTOS 宿主”的 1 ms 等待，核心逻辑在 vBmCoreTick()
    }
}

void vBmCoreInit(void)
{
    // 优先级 3：高于按键/浮点换算（2），低于软件定时器服务（4）
    xTaskCreate(vBmCoreTask, "BmCore", BMCORE_TASK_STACK_WORDS, NULL, 3, &xBmCoreTask);
    configASSERT(xBmCoreTask != NULL);
}

TaskHandle_t xBmCoreGetTaskHandle(void)
{
    return xBmCoreTask;
}
