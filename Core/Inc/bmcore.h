#ifndef BMCORE_H
#define BMCORE_H

#include "FreeRTOS.h"
#include "task.h"

// BmCore 任务栈：256 word = 1024 B，够 10 Hz DMM 浮点计算
#define BMCORE_TASK_STACK_WORDS 256

void vBmCoreTick(void); // 1 ms 调一次；RTOS 任务和裸机 main 循环共用同一套核心
#ifndef BM_BAREMETAL
void vBmCoreInit(void);
TaskHandle_t xBmCoreGetTaskHandle(void);
#endif

#endif
