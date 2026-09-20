/*
 * WaveformPanel.c
 *
 *  Created on: May 1, 2026
 *      Author: liuyu
 */

#include "lcd.h"
#include "scale_panel.h"
#include "cursor_panel.h"

// 重绘波形面板
void RepaintWaveformPanel(void)
{
	WaveformParamTypeDef waveformParam = {0};

	waveformParam.XScale = fGetXScale();
	waveformParam.YScale = fGetYScale();
	waveformParam.CursorX1 = GetCursorX1Pct();
	waveformParam.CursorX2 = GetCursorX2Pct();
	waveformParam.CursorY1 = GetCursorY1Pct();
	waveformParam.CursorY2 = GetCursorY2Pct();

	LCD_DrawWaveform(&waveformParam);
}
