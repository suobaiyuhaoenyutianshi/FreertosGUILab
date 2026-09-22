/*
 * general_panel.c —— 信号发生器面板（UI 层）
 *
 * 【这个文件负责什么】
 *   面板上只有两个可调参数：
 *     参数1 = 波形类型（OFF / Sin / Triangle / Square）
 *     参数2 = 频率（9 个固定挡位，见下面的 fWaveformFrequencys[]）
 *   本文件只做三件事：保存这两个参数、把它们画到屏上、按键后修改它们。
 *
 * 【和信号发生器（waveform_generator.c → DAC）的联系】
 *   本质是"UI 层 ↔ 驱动层"的分工，本文件不认识 DAC / TIM5，也不碰任何寄存器：
 *     本文件（UI）   ：只维护两个变量 wave / fwaveNowFrequency
 *     发生器（驱动） ：把这两个变量翻译成硬件动作——
 *                      填波形表 usWaveTable[] → 用 DMA 循环喂给 DAC → PA4 输出电压
 *   所以每次改动都通过两个 setter 通知发生器，发生器内部做"停 → 重算 → 重启"：
 *     vWaveGenSetShape(0~3)    → 换波形形状
 *     vWaveGenSetFrequency(Hz) → 重算采样率 fs 和"每周期点数" usWaveLength
 *
 * 【谁调用本文件的三个入口】
 *   按键模块（key/KEY*.c 的回调）：
 *     vSelected_general_panel()  —— 选中面板；再按一次在两个参数间切换光标
 *     vCancel_general_panel()    —— 取消选中（光标消失）
 *     vadjust_general_panel(±1)  —— KEY_UP / KEY_DOWN 传入 +1 / -1
 */

#include "general_panel.h"
#include "main.h"
#include "LCD_TASK.h"
#include <stdint.h>
#include <sys/_intsup.h>
#include "lcd.h"
#include "waveform_generator.h"

extern EventGroupHandle_t xEventLcd; // LCD 任务的事件组：置位 = 请求重绘某个部件

/* ---------------- 面板状态（全部状态就这三个） ---------------- */

// 光标位置：-1 = 未选中；0 = 选中"波形类型"；1 = 选中"频率"
static int8_t isCursor = -1;

// 当前波形类型。枚举编号和波形发生器的 ucWaveShape 一一对应：
// 0-OFF  1-Sin  2-Triangle  3-Square
static waveShap_t wave = WaveformShape_Off;

// 频率挡位表（9 档）。注意它和发生器内部的机制对应：
//   500Hz 及以下：发生器把采样率降下来（fs = 1000×f），每周期仍有 1000 个点
//   500Hz 以上  ：采样率顶到 500kHz 上限，只能减少"每周期点数"（f=50kHz 时只剩 10 点）
static float fWaveformFrequencys[] = {50.0f, 100.0f, 200.0f, 500.0f, 1.0e3f, 2.0e3f, 5.0e3f, 10.0e3f, 50.0e3f};
static uint8_t ucWaveformFrequencysNum = 9; // 挡位总数
static float fwaveNowFrequency = 1.0e3f;    // 当前频率，上电默认 1kHz

/*
 * @作用：把波形类型枚举翻译成屏幕上要显示的字符串
 * @返回：常数字符串（"OFF"/"Sin"/"Triangle"/"Square"）
 * @说明：纯显示用，和硬件无关
 */
static const char* pcGetwaveName(void){
    const char* pcwaveName = "Unknow" ;

	if(wave == WaveformShape_Off)
	{
		pcwaveName = "OFF"; // 关闭波形输出
	}
	else if(wave == WaveformShape_Sin)
	{
		pcwaveName = "Sin"; // 正弦波
	}
	else if(wave == WaveformShape_Triangle)
	{
		pcwaveName = "Triangle"; // 三角波
	}
	else if(wave == WaveformShape_Square)
	{
		pcwaveName = "Square"; // 方波
	}
    return pcwaveName;
}

/*
 * @作用：取消选中本面板
 * @说明：只改光标状态 + 请求重绘，不动信号输出（正在输出的波形继续输出）
 */
void vCancel_general_panel(void){
    isCursor = -1;
    xEventGroupSetBits(xEventLcd, GENERATOR_PANEL_REPAINT_BIT);
}

/*
 * @作用：选中本面板；已选中时在两个参数之间循环切换光标
 * @说明：同样只改光标 + 请求重绘
 */
void vSelected_general_panel(void){
    if (isCursor < 0) {
        isCursor = 0;
    } else {
        isCursor++;
        if (isCursor > 1) {
            isCursor = 0;
        }
    }
    xEventGroupSetBits(xEventLcd, GENERATOR_PANEL_REPAINT_BIT);
}

/*
 * @作用：把整个面板画出来（由 LCD 任务在 GENERATOR_PANEL_REPAINT_BIT 置位时调用）
 * @说明：只读面板状态，不改任何硬件；画的是"当前参数值"
 *       坐标含义：LCD 逻辑坐标 x 0~319、y 0~239，原点在左上角
 */
void vRepaint_geneeral_panel(void){
    // 1. 标题栏，坐标（260，184），宽60、高15，颜色#FF7BFF
	LCD_SetBrush(0xFF7BFF);
	LCD_SetCursor(260,184);
	LCD_FillRect(60, 15);

	// 2. 标题文字，坐标（262, 197），颜色#000000
	LCD_SetPen(0x000000);
	LCD_SetCursor(262,197);
	LCD_PrintString("Generato");

	// 3. 面板背景，坐标(260, 199)，宽60、高35，颜色#CCCCCC
	LCD_SetCursor(260, 199);
	LCD_SetBrush(0xCCCCCC);
	LCD_FillRect(60, 35);

    // 4. 光标：参数1被选中画在(262,202)，参数2被选中画在(262,212)，宽58、高10、天蓝色
    if (isCursor >= 0 && isCursor <= 1) {

		LCD_SetBrush(0x00ffff);

		if(isCursor == 0)
		{
			LCD_SetCursor(262,202);
		}
		else
		{
			LCD_SetCursor(262,212);
		}

		LCD_FillRect(58, 10);
    }

	// 5. 参数1（波形类型）：坐标（263，212），选中时用天蓝底，否则浅灰底
	const char *pcParameter1String = pcGetwaveName();

	if(isCursor == 0)
	{
		LCD_SetBrush(0x00ffff); // 被选中时，文字使用天蓝色背景
	}
	else
	{
		LCD_SetBrush(0xCCCCCC); // 未被选中，文字使用浅灰色背景
	}

	LCD_SetCursor(263, 212);
	LCD_PrintString(pcParameter1String); // 打印出来

	// 6. 参数2（频率）：坐标（263，222），用国际单位制打印（自动带 k 等单位）
	if( isCursor == 1)
	{
		LCD_SetBrush(0x00ffff); // 被选中时，文字使用天蓝色背景
	}
	else
	{
		LCD_SetBrush(0xCCCCCC); // 未被选中，文字使用浅灰色背景
	}

	LCD_SetCursor(263, 222);
	LCD_PrintFloatSI(fwaveNowFrequency, 6, "Hz"); // 例如 1kHz 会显示成 1.00000kHz
}

/*
 * @作用：按键调整（KEY_UP 传 +1，KEY_DOWN 传 -1），由按键回调调用
 * @参数：directNum - 调整方向/步数
 * @联系：
 *   光标在参数1 → 改波形类型 → vWaveGenSetShape()  → 发生器立刻换波形表并重启 DAC
 *   光标在参数2 → 改频率     → vWaveGenSetFrequency() → 发生器重算采样率与每周期点数
 * @注意：只在这里调用 setter，也就是"只有用户真按了键才动硬件"
 */
void vadjust_general_panel(int8_t directNum){
    if(isCursor < 0 || isCursor > 1) return; // 没选中任何参数，直接返回

    if (isCursor == 1) {
        // 参数2：先在挡位表里找到"当前频率"的位置，再按 ±1 移动，最后夹紧到表的两端
        int8_t i;
        for(i = 0; i < ucWaveformFrequencysNum; i++){
            if (fWaveformFrequencys[i] == fwaveNowFrequency) {
                break;
            }
        }
        i = i + directNum;
        if (i < 0) {
            i = 0;
        }
        if (i >= ucWaveformFrequencysNum) {
            i = ucWaveformFrequencysNum - 1;
        }
        fwaveNowFrequency = fWaveformFrequencys[i];
        vWaveGenSetFrequency(fwaveNowFrequency); // 通知信号发生器改频率（内部会重算表并重启 DAC）

    } else {
        // 参数1：4 种波形循环切换（+4 是为了配合负数取模，避免出现负下标）
        wave = (wave + directNum + 4) % 4;
        vWaveGenSetShape((uint8_t)wave);         // 通知信号发生器改波形（内部会重填表并重启 DAC）
    }

    xEventGroupSetBits(xEventLcd, GENERATOR_PANEL_REPAINT_BIT); // 参数变了，请求重绘本面板
}
