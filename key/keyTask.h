#ifndef TASK_KEYS_KEYTOYAOL_H
#define  TASK_KEYS_KEYTOYAOL_H
#include "key.h"
#include "KEY1.h"
/* 慢按键任务（KEYSLOW）的栈深度，单位：字。
 * 创建任务与内存监控注册共用这一个宏，避免两处数字不一致。 */
#define TASK_STACK_KEY_SLOW     128
void vKeyWorkerTask(void *pvArg);//消费者任务 队列
void vKeySlowTask(void *pvArg);//慢回调消费者：KEYU/KEYD 共用的唯一执行者
void VkeyTotalInit(void* pvArg);//是检测任务循环前的调用每个键的初始化  与创建消费者任务 队列QueueHandle_t xKeysQue 这个创建的大小是job_t 10个先
void vKeyScanStep(void);//单次扫描，由 BmCore 的 10 ms 分频调用
#endif