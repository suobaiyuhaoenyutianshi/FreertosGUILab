#ifndef __APP_USART1_DMA_H
#define __APP_USART1_DMA_H
#include "FreeRTOS.h"
#include "task.h"
#include "memory_monitor.h"
#include <stdarg.h>

/**
 * @brief 初始化 USART1 的 DMA 发送框架（动态内存 + 零拷贝）
 * @note  必须在 FreeRTOS 调度器启动前（vTaskStartScheduler 之前）调用
 */
void App_USART1_Init(void);

/**
 * @brief 格式化打印（线程安全，非阻塞业务任务）
 * @param fmt  格式化字符串（支持 %d, %s, %.2f 等标准格式）
 * @param ...  可变参数列表
 * @note  内部使用 vsnprintf 双重调用精确计算长度，只申请刚好够用的内存
 *        队列满了时，调用此函数的任务会阻塞等待
 */
void App_USART1_Printf(const char *fmt, ...);
extern TaskHandle_t usart1; 
#endif