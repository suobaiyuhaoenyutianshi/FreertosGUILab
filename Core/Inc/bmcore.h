#ifndef BMCORE_H
#define BMCORE_H

#include "FreeRTOS.h"
#include "task.h"

// BmCore 任务栈：256 word = 1024 B，够 10 Hz DMM 浮点计算
#define BMCORE_TASK_STACK_WORDS 256

void vBmCoreInit(void);
TaskHandle_t xBmCoreGetTaskHandle(void);

#endif
