#include "memory_monitor.h"
#include "portmacro.h"
#include "main.h"
#include "app_usart1_dma.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/_types.h>
//之后全部改成串口print

/** 存储被监控任务的句柄数组 */
static  TaskHandle_t xMonitoredTaskHandles[MAX_MONITORED_TASKS];
/** 当前已注册的任务数量 */
static uint8_t ucMonitoredTaskCount =0;
static void prvMemoryMonitorTask( void * pvParameters );
/**
 * @brief   注册一个任务，使其被内存监控任务跟踪栈使用情况
 * @param   xTask   要监控的任务句柄（创建任务时保存的那个句柄）
 */
void vRegisterTaskForMonitoring( TaskHandle_t xTask ){
    if(ucMonitoredTaskCount < MAX_MONITORED_TASKS){
        xMonitoredTaskHandles[ucMonitoredTaskCount] =xTask;
        ucMonitoredTaskCount++;
    }else {
        App_USART1_Printf( "Warning: Cannot register more tasks for monitoring (max %d)\n",
                MAX_MONITORED_TASKS );
    }
    

}

/**
 * @brief   启动内存监控任务（只需调用一次）
 */
void vStartMemoryMonitor( void ){
    xTaskCreate(prvMemoryMonitorTask,"MemMon"
        ,MONITOR_TASK_STACK_SIZE,NULL
        ,MONITOR_TASK_PRIORITY,NULL);
}
static void prvMemoryMonitorTask( void * pvParameters )
{
    (void)pvParameters;
    size_t xInitialFreeHeap = xPortGetFreeHeapSize();

    for( ;; )
    {
        size_t xFreeHeapNow = xPortGetFreeHeapSize();
        size_t xMinEverFreeHeap  = xPortGetMinimumEverFreeHeapSize();
        size_t xUsedHeap = configTOTAL_HEAP_SIZE - xFreeHeapNow;   // 修正
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

        // 输出任务栈信息
        App_USART1_Printf( "Task Stacks (High Water Mark = min free ever):\n" );
        if( ucMonitoredTaskCount == 0 )
        {
            
            App_USART1_Printf( "  No tasks registered.\n" );
        }
        else
        {
            for( int i = 0; i < ucMonitoredTaskCount; i++ )
            {
                TaskHandle_t xTask = xMonitoredTaskHandles[i];
                if( xTask == NULL )
                {
                    App_USART1_Printf( "  [Invalid handle] : skipped\n" );
                    continue;
                }
                const char* pcTaskName = pcTaskGetName(xTask);
                UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark( xTask );
                App_USART1_Printf( "  %-12s : min free %u words (%u bytes)\n",
                   pcTaskName,
                   ( unsigned ) uxHighWaterMark,
                   ( unsigned ) uxHighWaterMark * sizeof(StackType_t) );
                }
        }

        UBaseType_t uxMonitorTaskHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        App_USART1_Printf( "  %-12s : min free %u word (monitor itself)\n",
                           "MemMon", ( unsigned ) uxMonitorTaskHighWaterMark );
        App_USART1_Printf( "=====================================\n\n" );

        // 延时，控制输出频率
        vTaskDelay( pdMS_TO_TICKS( MONITOR_INTERVAL_MS ) );
    }
}