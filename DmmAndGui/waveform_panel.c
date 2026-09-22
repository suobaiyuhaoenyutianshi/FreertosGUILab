/*
 * WaveformPanel.c
 *
 *  Created on: May 1, 2026
 *      Author: liuyu
 */

#include "lcd.h"
#include "main.h"
#include "scale_panel.h"
#include "cursor_panel.h"
#include "waveFormCapture.h"
#include "dmm.h"

// 重绘波形面板
void RepaintWaveformPanel(void)
{
	WaveformParamTypeDef waveformParam = {0};

	waveformParam.Waveform = waveDate();                    // 双缓冲：拿到的是完整一帧，不需要加锁
	waveformParam.WaveLength = 1024;
	waveformParam.SampleRate = uWaveCaptureGetSampleRate(); // 采样率随时基联动，必须动态取
	waveformParam.Trigger = xDMMGetData().fTriggerLevel;    // 触发电平（注入组第 4 路）
	waveformParam.XScale = fGetXScale();
	waveformParam.YScale = fGetYScale();
	waveformParam.CursorX1Pct = GetCursorX1Pct();
	waveformParam.CursorX2Pct = GetCursorX2Pct();
	waveformParam.CursorY1Pct = GetCursorY1Pct();
	waveformParam.CursorY2Pct = GetCursorY2Pct();

	LCD_DrawWaveform(&waveformParam);
}
