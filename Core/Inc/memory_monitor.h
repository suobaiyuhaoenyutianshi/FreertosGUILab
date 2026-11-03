#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H
/******************************************************************************
 * 文件：memory_monitor.h
 * 描述：FreeRTOS 内存监控模块头文件
 *       - 打印堆的整体占用与历史最低剩余
 *       - 打印每个被注册任务的"栈剩余量"（high water mark）
 ******************************************************************************/
#include "FreeRTOS.h"
#include "task.h"

/** 监控任务自身的栈大小（单位：字，不是字节！） */
#ifndef MONITOR_TASK_STACK_SIZE
    #define MONITOR_TASK_STACK_SIZE (256)
#endif
/** 监控任务的优先级，通常设为较低，避免影响关键任务 */
#ifndef MONITOR_TASK_PRIORITY
    #define MONITOR_TASK_PRIORITY (1)
#endif
/** 监控周期，单位毫秒 */
#ifndef MONITOR_INTERVAL_MS
    #define MONITOR_INTERVAL_MS (2000)
#endif
/** 最多监控的任务数量 */
#ifndef MAX_MONITORED_TASKS
    #define MAX_MONITORED_TASKS        ( 10 )
#endif
/** 剩余栈占比低于这个百分比就打 "余量偏低" 标记，提醒扩栈 */
#ifndef MONITOR_LOW_STACK_PERCENT
    #define MONITOR_LOW_STACK_PERCENT   ( 20 )
#endif

/**
 * @brief   注册一个任务，使其被内存监控任务跟踪栈使用情况
 * @param   xTask            要监控的任务句柄（创建任务时保存的那个句柄）
 * @param   uxStackSizeWords 该任务在 xTaskCreate 时申请的栈深度（单位：字）
 *                           —— FreeRTOS 只能给出"还剩多少"，给不出"原本给了多少"，
 *                              所以必须由调用方把申请值一起报进来，才能算剩余百分比。
 */
void vRegisterTaskForMonitoring( TaskHandle_t xTask, UBaseType_t uxStackSizeWords );

/**
 * @brief   启动内存监控任务（只需调用一次）
 */
void vStartMemoryMonitor( void );
#endif
