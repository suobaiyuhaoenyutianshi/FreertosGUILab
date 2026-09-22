#ifndef GUI_WAVEFORM_GENERATOR_H_
#define GUI_WAVEFORM_GENERATOR_H_

#include <stdint.h>

//波形形状，与 general_panel.h 的 waveShap_t 一一对应
//0 - OFF，1 - Sin，2 - Triangle，3 - Square

//初始化信号发生器（DAC + TIM5 + DMA，输出引脚 PA4/DAC_OUT1）
void vWaveGenInit(void);

//设置波形形状
void vWaveGenSetShape(uint8_t ucShape);

//设置波形频率，单位 Hz
void vWaveGenSetFrequency(float fFrequency);

#endif
