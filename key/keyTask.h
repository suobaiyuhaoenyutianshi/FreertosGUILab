#ifndef TASK_KEYS_KEYTOYAOL_H
#define  TASK_KEYS_KEYTOYAOL_H
#include "key.h"
#include "KEY1.h"
void vKeyWorkerTask(void *pvArg);//消费者任务 队列
void VkeyTotalInit(void* pvArg);//是检测任务循环前的调用每个键的初始化  与创建消费者任务 队列QueueHandle_t xKeysQue 这个创建的大小是job_t 10个先
void vKeyWOrksScan(void* pvArg);//检测任务
#endif