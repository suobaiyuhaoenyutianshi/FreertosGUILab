#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H
/******************************************************************************
 * 文件：memory_monitor.h
 * 描述：FreeRTOS 内存监控模块头文件
 *       - 提供堆内存和任务栈的监控功能
 *       - 用户可调用 vRegisterTaskForMonitoring() 注册需要监控的任务
 *       - 调用 vStartMemoryMonitor() 启动监控任务
 ******************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
/** 监控任务自身的栈大小（单位：字，不是字节！具体取决于平台）
 *  建议给足，因为使用了 printf 可能消耗较多栈   我改成了串口输出*/
    //这种都是未定义的写法 方便以后的配置修改 例MONITOR_TASK_STACK_SIZE监控任务堆栈大小

#ifndef MONITOR_TASK_STACK_SIZE
    #define MONITOR_TASK_STACK_SIZE (256)//最小栈大小
#endif
/** 监控任务的优先级，通常设为较低，避免影响关键任务 */
#ifndef  MONITOR_TASK_PRIORITY
    #define  MONITOR_TASK_PRIORITY (1)//比空闲任务高一级
#endif
/** 监控周期，单位毫秒 */
#ifndef MONITOR_INTERVAL_MS
    #define MONITOR_INTERVAL_MS (2000)
#endif
/** 最多监控的任务数量 */
#ifndef MAX_MONITORED_TASKS
    #define MAX_MONITORED_TASKS        ( 10 )
#endif
/**
 * @brief   注册一个任务，使其被内存监控任务跟踪栈使用情况
 * @param   xTask   要监控的任务句柄（创建任务时保存的那个句柄）
 */
void vRegisterTaskForMonitoring( TaskHandle_t xTask );

/**
 * @brief   启动内存监控任务（只需调用一次）
 */
void vStartMemoryMonitor( void );
#endif