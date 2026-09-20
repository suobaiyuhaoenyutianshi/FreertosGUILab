#ifndef OSCILLOSCOPE_KEYS_KEYU_H
#define OSCILLOSCOPE_KEYS_KEYU_H
#include "key.h"
void keyUScan(void);
void keyUInit(void);
void vKeyUTask(void *pv);
extern key_Handle KeyU;
//extern SemaphoreHandle_t xKeyUSem;   // 供等待任务 xSemaphoreTake 使用
#endif