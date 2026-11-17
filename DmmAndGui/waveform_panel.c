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
#include <string.h>

/* ==================== 【临时测量】波形面板绘制耗时 ====================
 * 目的：确认一次 LCD_DrawWaveform() 是否能在双缓冲的安全窗口内画完。
 *   安全窗口 ≈ 2 × 两次发布间隔（当前时基下约 2 × 14ms = 28ms）
 *   绘制耗时 > 窗口 → 浮点任务会回卷写到正在画的那一块 → 波形撕裂
 * 用法：调试器 Live Watch / Watch 观察下面四个全局变量
 *   g_waveDrawMsMax      单次绘制最长耗时(ms)  ← 关键值，和 28 比
 *   g_waveDrawMsLast     最近一次绘制耗时(ms)
 *   g_wavePeriodMsMax    两次重绘的最长间隔(ms)，即实际刷新周期
 *   g_wavePeriodMsLast   最近一次重绘间隔(ms)
 * 分辨率 1ms（HAL_GetTick，TIM6 时基），对 28ms 这个判据足够。
 * 测完删掉这段和函数里的计时代码即可。
 * ================================================================== */
volatile uint32_t g_waveDrawMsLast   = 0;
volatile uint32_t g_waveDrawMsMax    = 0;
volatile uint32_t g_wavePeriodMsLast = 0;
volatile uint32_t g_wavePeriodMsMax  = 0;

static uint32_t uLastRepaintTick = 0;

/* 绘制快照：整帧拷一份私有副本再画。
 * 为什么要它：LCD_DrawWaveform 一次要画上百毫秒，远超"两层双缓冲"能挡住的窗口
 * （2 × 发布间隔 ≈ 28ms）。直接拿 waveDate() 的指针画，浮点任务会在这期间回卷、
 * 把正在画的那一块覆盖若干次，画面横向出现"半新半旧"的跳变。
 * 拷贝只花几十 µs（远小于一次发布间隔 14ms），拷完之后 LCD 再不碰共享缓冲，
 * 浮点任务怎么翻索引都不影响这一帧。
 * 代价：静态多占 4KB RAM。 */
static float fDrawBuffer[1024];

// 重绘波形面板
void RepaintWaveformPanel(void)
{
	WaveformParamTypeDef waveformParam = {0};

	uint32_t uTickStart = HAL_GetTick();   // 计时起点

	/* 先拍快照，再画快照：把"共享数据的读窗口"从上百毫秒压缩到几十微秒 */
	memcpy(fDrawBuffer, waveDate(), sizeof(fDrawBuffer));

	waveformParam.Waveform = fDrawBuffer;                    // 指向私有快照，不再指向双缓冲
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

	/* 计时收尾：把结果记进全局变量，供调试器观察 */
	uint32_t uTickEnd = HAL_GetTick();
	g_waveDrawMsLast = uTickEnd - uTickStart;
	if (g_waveDrawMsLast > g_waveDrawMsMax) {
		g_waveDrawMsMax = g_waveDrawMsLast;
	}
	/* 两次重绘的间隔（首次 uLastRepaintTick=0，会得到一个无意义的大值，忽略即可） */
	g_wavePeriodMsLast = uTickStart - uLastRepaintTick;
	if (g_wavePeriodMsLast > g_wavePeriodMsMax) {
		g_wavePeriodMsMax = g_wavePeriodMsLast;
	}
	uLastRepaintTick = uTickStart;
}
