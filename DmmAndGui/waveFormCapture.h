#ifndef GUI_WAVE_CAPTURE_H_
#define GUI_WAVE_CAPTURE_H_
#include"stdint.h"

//初始化波形捕捉器：TIM3 采样时钟、两个软件定时器、浮点换算任务
void WaveCaptureInit(void);

//获取当前可显示的波形数据（1024 点，单位 V，双缓冲中"已发布"的那一块）
float * waveDate(void);

//设置采样率（改 TIM3 的 PSC/ARR，1024 点对应一屏）
void WaveCaptureSetSampleRate(uint16_t Psc, uint16_t Arr);

//获取当前采样率，单位 Hz
uint32_t uWaveCaptureGetSampleRate(void);

#endif
