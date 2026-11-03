#ifndef OSCILLOSCOPE_KEYS_KEYU_H
#define OSCILLOSCOPE_KEYS_KEYU_H
#include "key.h"
void keyUScan(void);
void keyUInit(void);
/* KEYU 的专职执行任务：等 xKeyUSem，收到一次按下就调用一次 keyUCallback */
void vKeyUTask(void *pv);
extern key_Handle KeyU;
/* 供等待任务 xSemaphoreTake 使用；由 keyUInit() 在扫描任务里创建（计数型，上限 4） */
extern SemaphoreHandle_t xKeyUSem;
#endif
