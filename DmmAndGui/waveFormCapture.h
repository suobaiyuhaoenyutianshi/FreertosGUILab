#ifndef GUI_WAVE_CAPTURE_H_
#define GUI_WAVE_CAPTURE_H_
#include"stdint.h"
#include "FreeRTOS.h"
#include "task.h"

/* 浮点换算任务的栈深度（单位：字）。
 * 创建任务和内存监控注册共用这一个宏：如果两处各写一个 128，
 * 以后改了一处忘了另一处，监控算出来的占用百分比就是错的。 */
#define WAVE_CAPTURE_TASK_STACK_WORDS   ( 128 )

//初始化波形捕捉器：TIM3 采样时钟、两个软件定时器、浮点换算任务
void WaveCaptureInit(void);

/* 取浮点换算任务（float_archIT）的句柄，供内存监控注册。
 * 它是示波器最重的业务任务：每帧要对 1024 个 ADC 码值做浮点换算，
 * 之前没进监控名单，等于监控里缺了最该看的那一项。 */
TaskHandle_t xWaveCaptureGetTaskHandle(void);

//获取当前可显示的波形数据（1024 点，单位 V，双缓冲中"已发布"的那一块）
float * waveDate(void);

//设置采样率（改 TIM3 的 PSC/ARR，1024 点对应一屏）
void WaveCaptureSetSampleRate(uint16_t Psc, uint16_t Arr);

//获取当前采样率，单位 Hz
uint32_t uWaveCaptureGetSampleRate(void);

#endif
