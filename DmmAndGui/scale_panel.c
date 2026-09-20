#include "scale_panel.h"
#include "main.h"
#include "LCD_TASK.h"
#include "lcd.h"
int iSelectedIndex = -1; // 光标位置，负数表示未转中，0-x轴缩放比，1-y轴缩放比
// x轴缩放比
static int16_t xScaleIndex = 5; // 当前挡位
static const float xScales[] = { 10.0e-6f, 20.0e-6f, 50.0e-6f, 100.0e-6f, 200.0e-6f, 500.0e-6f, 1.0e-3f, 2.0e-3f, 5.0e-3f, 10.0e-3f, 20.0e-3f, 50.0e-3f};
static const uint16_t numOfXScales = sizeof(xScales) / sizeof(float); // 挡位总数

// y轴缩放比
static int16_t yScaleIndex = 4; // 当前挡位
static const float yScales[] = { 50.0e-3f, 100.0e-3f, 200.0e-3f, 500.0e-3f, 1.0f };
static const uint16_t numOfYScales = sizeof(yScales) / sizeof(float); // 挡位总数
extern EventGroupHandle_t xEventLcd;
void vCancel_SCALE_panel(void){
    if (iSelectedIndex>=0) {
        iSelectedIndex =-1;
    }
    xEventGroupSetBits(xEventLcd,SCALE_PANEL_REPAINT_BIT);
}
void vSelected_SCALE_panel(void){
    if (iSelectedIndex<0) {
        iSelectedIndex=0;
    }else {
        iSelectedIndex = ((iSelectedIndex+1)+2)%2;
        
    }
    xEventGroupSetBits(xEventLcd,SCALE_PANEL_REPAINT_BIT);
}
void vadjust_SCALE_panel(int8_t directNum){
    if (iSelectedIndex<0) {
        return;
    }
    if (iSelectedIndex == 0) {
        xScaleIndex = (xScaleIndex + directNum +numOfXScales)%numOfXScales;

    }else {
        yScaleIndex = (yScaleIndex+directNum+numOfYScales)%numOfYScales;
    }

     xEventGroupSetBits(xEventLcd,SCALE_PANEL_REPAINT_BIT|SCALE_LABEL_REPAINT_BIT);
}
void vRepaint_SCALE_panel(void){
    
	// 1. 标题栏，坐标（260, 67），宽60、高15，颜色#FF7BFF
	LCD_SetBrush(0xFF7BFF);
	LCD_SetCursor(260, 67);
	LCD_FillRect(60, 15);

	// 2. 标题文字，坐标（275, 80），颜色#000000
	LCD_SetPen(0x000000);
	LCD_SetCursor(275, 80);
	LCD_PrintString("Scale");

	// 3. 面板背景，坐标(260, 82)，宽60、高35，颜色#CCCCCC
	LCD_SetCursor(260, 82);
	LCD_SetBrush(0xCCCCCC);
	LCD_FillRect(60, 35);

	// 4. 绘制参数1，x
	if(iSelectedIndex == 0) // 参数X1被选中
	{
		// 绘制光标，(274, 89)，宽9，高11，#00ffff
		LCD_SetBrush(0x00FFFF);
		LCD_SetCursor(274, 89);
		LCD_FillRect(9, 11);
	}
	else
	{
		LCD_SetBrush(0xCCCCCC);
	}
	LCD_SetCursor(275, 98);
	LCD_PrintString("x");

	// 5. 绘制参数2，y
	if(iSelectedIndex == 1) // 参数X1被选中
	{
		// 绘制光标，(294, 89)，宽9，高11，#00ffff
		LCD_SetBrush(0x00FFFF);
		LCD_SetCursor(294, 89);
		LCD_FillRect(9, 11);
	}
	else
	{
		LCD_SetBrush(0xCCCCCC);
	}
	LCD_SetCursor(295, 98);
	LCD_PrintString("y");

}
//
// @作用：获取X轴向的缩放比
//
float fGetXScale(void)
{
	return xScales[xScaleIndex];
}

//
// @作用：获取Y轴向的缩放比
//
float fGetYScale(void)
{
	return yScales[yScaleIndex];
}