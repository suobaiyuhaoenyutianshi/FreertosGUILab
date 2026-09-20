#include "general_panel.h"
#include "main.h"
#include "LCD_TASK.h"
#include <stdint.h>
#include <sys/_intsup.h>
#include "lcd.h"
extern EventGroupHandle_t xEventLcd;
//这些是按钮按发出的
//取消选中，放在其他按钮
//cursor  小于0 没选中    0标签1  1标签2
static int8_t isCursor = -1; 
static waveShap_t wave= WaveformShape_Off;
static float fWaveformFrequencys[] =  {50.0f, 100.0f, 200.0f, 500.0f, 1.0e3f, 2.0e3f, 5.0e3f, 10.0e3f, 50.0e3f };
static uint8_t ucWaveformFrequencysNum = 9;
static float fwaveNowFrequency =  1.0e3f;
/*const 在 * 左边：修饰指向的内容，如 const char *p。

const 在 * 右边：修饰指针本身，如 char * const p。*/
static const char* pcGetwaveName(void){
    const char* pcwaveName = "Unknow" ;
   
	if(wave== WaveformShape_Off)
	{
		pcwaveName = "OFF"; // 关闭波形输出
	}
	else if(wave== WaveformShape_Sin)
	{
		pcwaveName = "Sin"; // 正弦波
	}
	else if(wave== WaveformShape_Triangle)
	{
		pcwaveName= "Triangle"; // 三角波
	}
	else if(wave == WaveformShape_Square)
	{
		pcwaveName= "Square"; // 方波
	}
        return pcwaveName;

}
void vCancel_general_panel(void){
    isCursor =-1;
    xEventGroupSetBits(xEventLcd,GENERATOR_PANEL_REPAINT_BIT);
}
//选中 这两个放在本按钮
void vSelected_general_panel(void){
    if (isCursor<0) {
        isCursor = 0;
    }else  {
       isCursor++;
       if (isCursor>1) {
        isCursor =0;
       }
    }
    xEventGroupSetBits(xEventLcd,GENERATOR_PANEL_REPAINT_BIT);

}
//重花 不是只有光标选中才可以改变 而是变化
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

    
    
    // 4. 光标，参数1被选中：(262,202)，宽58，高10，颜色#00FFFF
    //        参数2被选中：(262,212)，宽58，高10，颜色#00FFFF 记住是左下角
    if (isCursor>=0&&isCursor <=1) {
        
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
    // 5. 参数1，坐标（263，212）

	// 获取参数1的文本
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

	// 6. 参数2，坐标（263，222）
	if( isCursor== 1)
	{
		LCD_SetBrush(0x00ffff); // 被选中时，文字使用天蓝色背景
	}
	else
	{
		LCD_SetBrush(0xCCCCCC); // 未被选中，文字使用浅灰色背景
	}

	LCD_SetCursor(263, 222);
	LCD_PrintFloatSI(fwaveNowFrequency, 6, "Hz"); // 使用国际单位制绘制频率值
    



}
//调整  放在key_down与up
void vadjust_general_panel(int8_t directNum){
    if(isCursor <0 || isCursor>1)return;
    if (isCursor ==1) {
        int8_t i =0;
        for(i;i<ucWaveformFrequencysNum;i++){
            if (fWaveformFrequencys[i] == fwaveNowFrequency) {
                break;
            }
        }
        i = i+directNum;
        if (i<0) {
            i=0;
        }if (i >= ucWaveformFrequencysNum) {
            i= ucWaveformFrequencysNum -1;
        }
        fwaveNowFrequency =fWaveformFrequencys[i];

    }else{
        //改变波形
        wave = (wave + directNum +4)%4;


    } 

    xEventGroupSetBits(xEventLcd,GENERATOR_PANEL_REPAINT_BIT);


}