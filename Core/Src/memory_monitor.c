#include "memory_monitor.h"
#include "main.h"
#include "app_usart1_dma.h"
#include <stddef.h>   /* size_t 的公开声明：原来用的是 <sys/_types.h>，
                       * 那是 newlib 的"内部头文件"，不属于 C 标准库的稳定接口，
                       * 换编译器/换 libc（比如换成 ARMCLANG 或 picolibc）就可能找不到，
                       * 而本文件只需要 size_t，用 <stddef.h> 才是可移植的写法。 */
#include <stdint.h>

/** 存储被监控任务的句柄数组 */
static TaskHandle_t xMonitoredTaskHandles[MAX_MONITORED_TASKS];
/** 与上面一一对应的"申请栈深度"（单位：字），用于算剩余百分比 */
static UBaseType_t uxMonitoredTaskStackWords[MAX_MONITORED_TASKS];
/** 当前已注册的任务数量 */
static uint8_t ucMonitoredTaskCount = 0;

static void prvMemoryMonitorTask( void * pvParameters );

/**
 * @brief   注册一个任务，使其被内存监控任务跟踪栈使用情况
 * @param   xTask            要监控的任务句柄
 * @param   uxStackSizeWords 该任务申请时的栈深度（字）
 *
 * 说明（为什么加这两道校验）：
 *   1) 传 NULL 进去什么也监控不到，老代码只是把它放进数组、等打印时才提示
 *      "[Invalid handle]"，等于静默失效。这里在注册当场就拒绝，问题暴露在启动阶段。
 *   2) 同一个句柄重复注册会白白占掉一个槽位。而槽位上限只有 10 个，
 *      一旦满了后面的任务就再也注册进来，监控名单会"悄悄少人"。
 */
void vRegisterTaskForMonitoring( TaskHandle_t xTask, UBaseType_t uxStackSizeWords ){
    if( xTask == NULL )
    {
        App_USART1_Printf( "Warning: register a NULL task handle, ignored\n" );
        return;
    }

    for( uint8_t i = 0; i < ucMonitoredTaskCount; i++ )
    {
        if( xMonitoredTaskHandles[i] == xTask )
        {
            return;   /* 已注册过，直接忽略 */
        }
    }

    if( ucMonitoredTaskCount < MAX_MONITORED_TASKS )
    {
        xMonitoredTaskHandles[ucMonitoredTaskCount] = xTask;
        uxMonitoredTaskStackWords[ucMonitoredTaskCount] = uxStackSizeWords;
        ucMonitoredTaskCount++;
    }
    else
    {
        /* 槽位耗尽时用任务名提示，方便直接把 MAX_MONITORED_TASKS 调大 */
        App_USART1_Printf( "Warning: Cannot register more tasks (max %d), %s skipped\n",
                MAX_MONITORED_TASKS, pcTaskGetName( xTask ) );
    }
}

/**
 * @brief   启动内存监控任务（只需调用一次）
 */
void vStartMemoryMonitor( void ){
    configASSERT( xTaskCreate( prvMemoryMonitorTask,"MemMon"
        ,MONITOR_TASK_STACK_SIZE,NULL
        ,MONITOR_TASK_PRIORITY,NULL) == pdPASS );
}

/* 打印一个任务的栈余量。
 * uxTaskGetStackHighWaterMark() 返回的是"历史上剩余栈的最小值"（单位：字），
 * 也就是这个任务跑到现在"最紧张的那一刻还剩多少"——拿它判断会不会溢出最直观。
 * 注意它只在 INCLUDE_uxTaskGetStackHighWaterMark == 1 时才编译进来（本项目已打开）。
 */
static void prvPrintOneTaskStack( TaskHandle_t xTask, UBaseType_t uxTotalWords )
{
    const char* pcTaskName = pcTaskGetName( xTask );
    UBaseType_t uxFreeWords = uxTaskGetStackHighWaterMark( xTask );

    if( uxTotalWords == 0 )
    {
        App_USART1_Printf( "  %-12s : free %4u w (%5u B), total unknown\n",
                           pcTaskName,
                           ( unsigned ) uxFreeWords,
                           ( unsigned )( uxFreeWords * sizeof( StackType_t ) ) );
        return;
    }

    unsigned uPercent = ( unsigned )( ( ( unsigned ) uxFreeWords * 100u ) / ( unsigned ) uxTotalWords );
    const char* pcFlag = ( uPercent <= MONITOR_LOW_STACK_PERCENT ) ? "  <-- 余量偏低，考虑扩栈" : "";

    App_USART1_Printf( "  %-12s : free %3u/%-3u w (%5u B), %3u%%%s\n",
                       pcTaskName,
                       ( unsigned ) uxFreeWords,
                       ( unsigned ) uxTotalWords,
                       ( unsigned )( uxFreeWords * sizeof( StackType_t ) ),
                       uPercent,
                       pcFlag );
}

static void prvMemoryMonitorTask( void * pvParameters )
{
    (void)pvParameters;
    size_t xInitialFreeHeap = xPortGetFreeHeapSize();

    for( ;; )
    {
        size_t xFreeHeapNow = xPortGetFreeHeapSize();
        size_t xMinEverFreeHeap  = xPortGetMinimumEverFreeHeapSize();
        size_t xUsedHeap = configTOTAL_HEAP_SIZE - xFreeHeapNow;
        size_t xInitialUsedHeap = configTOTAL_HEAP_SIZE - xInitialFreeHeap;
        size_t xHeapGrowth = ( xUsedHeap > xInitialUsedHeap ) ?
                             ( xUsedHeap - xInitialUsedHeap ) : 0;

        // 输出堆信息
        App_USART1_Printf( "\n========== Memory Monitor ==========\n" );
        App_USART1_Printf( "Heap:\n" );
        App_USART1_Printf( "  Total       : %u bytes\n", ( unsigned ) configTOTAL_HEAP_SIZE );
        App_USART1_Printf( "  Free now    : %u bytes\n", ( unsigned ) xFreeHeapNow );
        App_USART1_Printf( "  Min ever    : %u bytes\n", ( unsigned ) xMinEverFreeHeap );
        App_USART1_Printf( "  Used now    : %u bytes\n", ( unsigned ) xUsedHeap );
        App_USART1_Printf( "  Growth      : %u bytes\n", ( unsigned ) xHeapGrowth );

        // 输出任务栈信息：min free 越小说明这个任务离溢出越近
        App_USART1_Printf( "Task Stacks (free = 历史最少剩余，单位 word=4B):\n" );
        if( ucMonitoredTaskCount == 0 )
        {
            App_USART1_Printf( "  No tasks registered.\n" );
        }
        else
        {
            for( int i = 0; i < ucMonitoredTaskCount; i++ )
            {
                prvPrintOneTaskStack( xMonitoredTaskHandles[i], uxMonitoredTaskStackWords[i] );
            }
        }

        // 监控任务自己也算一个：不判断它的话，打印时会把自己的栈撑爆而没人知道
        prvPrintOneTaskStack( xTaskGetCurrentTaskHandle(), MONITOR_TASK_STACK_SIZE );
        App_USART1_Printf( "=====================================\n\n" );

        // 延时，控制输出频率
        vTaskDelay( pdMS_TO_TICKS( MONITOR_INTERVAL_MS ) );
    }
}
