#include "Run_mode_panel.h"
#include "main.h"
#include "LCD_TASK.h"
#include "lcd.h"
#include <stdint.h>
#include <sys/_intsup.h>
extern EventGroupHandle_t xEventLcd;
//光标 小于0没选中 0选中只有一个参数
static int8_t isCursor = -1;
static uint8_t ucRUN_MODE_place =0 ;



void vCancel_RUN_mode_panel(void){
    isCursor = -1;

    xEventGroupSetBits(xEventLcd,RUNMODE_PANEL_REPAINT_BIT);

}

void vSelected_RUN_mode_panel(void){
    if (isCursor<0) {
        isCursor =0;
        xEventGroupSetBits(xEventLcd,RUNMODE_PANEL_REPAINT_BIT);
    }
}

void vRepaint_RUN_mode_panel(void){

    // 1. 标题栏，坐标（260，8），宽60、高15，颜色#FF7BFF
	LCD_SetBrush(0xFF7BFF);
	LCD_SetCursor(260,8);
	LCD_FillRect(60, 15);

	// 2. 标题文字，坐标（262, 21），颜色#000000
	LCD_SetPen(0x000000);
	LCD_SetCursor(262,21);
	LCD_PrintString("Run Mode");

	// 3. 面板背景，坐标(260, 23)，宽60、高35，颜色#CCCCCC
	LCD_SetCursor(260, 23);
	LCD_SetBrush(0xCCCCCC);
	LCD_FillRect(60, 35);

	// 4. 光标,(264,34)，宽51，高11，颜色#00FFFF
	if(isCursor == 0)
	{
		LCD_SetBrush(0x00ffff);
		LCD_SetCursor(262,34);
		LCD_FillRect(51, 11);
	}

	// 5. 参数1（运行模式），坐标（265, 43）
	LCD_SetCursor(265, 43);

	if(isCursor == 0)
	{
		LCD_SetBrush(0x00ffff); // 被选中时，文字使用天蓝色背景
	}
	else
	{
		LCD_SetBrush(0xCCCCCC); // 未被选中，文字使用浅灰色背景
	}

	if(ucRUN_MODE_place== 0)
	{
		LCD_PrintString("Running");
	}
	else
	{
		LCD_PrintString("One-Shot");
	}


}

void vadjust_RUN_mode_panel(int8_t directNum){
    if (isCursor<0) {
        return;
    }
    ucRUN_MODE_place =(ucRUN_MODE_place+directNum +2 )%2;
      xEventGroupSetBits(xEventLcd,RUNMODE_PANEL_REPAINT_BIT);


}