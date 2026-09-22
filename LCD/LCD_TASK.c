#include "LCD_TASK.h"
#include "Run_mode_panel.h"

#include "main.h"
#include "lcd.h"
#include "cursor_label.h"
#include "portmacro.h"
#include "projdefs.h"
#include <stdint.h>
#include "label_pwr.h"
#include "label_dmm.h" 
#include "general_panel.h" 
#include "cursor_panel.h"
#include "scale_panel.h"
#include "scale_label.h"
#include "Run_mode_panel.h"
#include "waveform_panel.h"
//extern const uint16_t image1[160 * 120];
//extern const uint16_t image2[160 * 120];
static SemaphoreHandle_t xSemForDMa;
static void PrvSendCommand(uint8_t pData);
static void PrvSendData(uint8_t* pData,uint16_t size);
static void prvShowImage(uint16_t* image);
static void Reset(void);
EventGroupHandle_t xEventLcd;
//LCD屏共有320行，240列
static void ClearScreen(void)
{
	
    // 240 像素宽 × 10 行 × 2 字节 = 4800 字节，静态分配
    static const uint8_t blackBuf[240 * 10 * 2] = {0};

    // 全屏窗口：列 0~239，行 0~319
   PrvSendCommand(0x2a);
    PrvSendData((uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4);

    PrvSendCommand(0x2b);
    PrvSendData((uint8_t[]){0x00, 0x00, 0x01, 0x3F}, 4);

    PrvSendCommand(0x2C);


    // 240×320 像素 = 76800 像素，每次发 2400 像素，共 32 次
    for(int i = 0; i < 32; i++)
    {
        PrvSendData((uint8_t *)blackBuf, sizeof(blackBuf));
    }
}




static void BackLightControl(uint8_t On)
{
	if(On != 0)
	{
		HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
	}

}
static void Reset(void){
	HAL_GPIO_WritePin(lcdRtS_GPIO_Port,lcdRtS_Pin,GPIO_PIN_RESET);
	vTaskDelay(pdMS_TO_TICKS(1));
	HAL_GPIO_WritePin(lcdRtS_GPIO_Port,lcdRtS_Pin,GPIO_PIN_SET);
	vTaskDelay(pdMS_TO_TICKS(125));

}
static void PrvSendCommand(uint8_t pData){
	
	// 切换到命令模式
	HAL_GPIO_WritePin(LCD_RS_GPIO_Port,LCD_RS_Pin,GPIO_PIN_RESET);
	//选择从机
	HAL_GPIO_WritePin(LCD_NSS_GPIO_Port,LCD_NSS_Pin,GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1,&pData,1,HAL_MAX_DELAY);;
	//取消选中
	HAL_GPIO_WritePin(LCD_NSS_GPIO_Port,LCD_NSS_Pin,GPIO_PIN_SET);

}
static void PrvSendData(uint8_t* pData,uint16_t size){
	// 切换到数据模式
	HAL_GPIO_WritePin(LCD_RS_GPIO_Port,LCD_RS_Pin,GPIO_PIN_SET);
	//选择从机
	HAL_GPIO_WritePin(LCD_NSS_GPIO_Port,LCD_NSS_Pin,GPIO_PIN_RESET);
	HAL_SPI_Transmit_DMA(&hspi1,pData,size);//只有dma完成会触发HAL_SPI_TxCpltCallback这个中断说明数据发射王  给信号下面才能进行
	xSemaphoreTake(xSemForDMa,portMAX_DELAY);

	//取消选中
	HAL_GPIO_WritePin(LCD_NSS_GPIO_Port,LCD_NSS_Pin,GPIO_PIN_SET);

}

void vLCDTask(void *pvParameters)
{
	//创建事件组
	xEventLcd = xEventGroupCreate();
	LCD_InitTypeDef lcdInstruct ={0};
	xSemForDMa = xSemaphoreCreateBinary();
	(void) pvParameters;
	BackLightControl(1);
	/*Reset();
	PrvSendCommand(0x11);
	vTaskDelay(pdMS_TO_TICKS(125)); // 等待125ms，保证已经苏醒
// 2. 0x3a 0x55 设置颜色传输格式为RGB656
	PrvSendCommand(0x3a);
	PrvSendData((uint8_t[]){0x55}, 1);
// 3. 0x20 关闭反向显示
	PrvSendCommand(0x20);
// 4. 0x29 开启屏幕显示
	PrvSendCommand(0x29); 
	ClearScreen();*/
		lcdInstruct.send_command_callback = PrvSendCommand;
	lcdInstruct.send_data_callback = PrvSendData;
	lcdInstruct.reset_callback = Reset;
	LCD_Clear();
	LCD_Init(&lcdInstruct);
	lcdInstruct.send_command_callback(0x20);//不能删
	LCD_SetBrush(0xFFFFFF); // 白色背景
	// 开机后的第一次，所有部件都需要重绘
	xEventGroupSetBits(xEventLcd, ALL_REPAINT_BITS);
	LCD_Clear(); // 清屏
	//LCD_SetPen(0x00ff00); // 绿色字
	//LCD_SetCursor(120, 120);
	//LCD_PrintString("Hello world");
	for(;;)
	{	// testLcd++;
		EventBits_t allKeys;
		allKeys = xEventGroupWaitBits(xEventLcd,ALL_REPAINT_BITS,pdTRUE,pdFALSE,portMAX_DELAY);
		//vTaskDelay(pdMS_TO_TICKS(10));
			/*prvShowImage(image1);
		vTaskDelay(pdMS_TO_TICKS(200));
		prvShowImage(image2);
		vTaskDelay(pdMS_TO_TICKS(200));*/
		/*LCD_SetCursor(80, 60);
    LCD_DrawBitmap(160, 120, (const uint8_t *)image1);
    vTaskDelay(pdMS_TO_TICKS(200));

    LCD_SetCursor(80, 60);
    LCD_DrawBitmap(160, 120, (const uint8_t *)image2);
    vTaskDelay(pdMS_TO_TICKS(200));
	*/
	  
		if (PWR_LABLE_REPAINT_BIT&allKeys) {
			PwrLabel_Repaint();
		}
		if(DMM_LABLE_REPAINT_BIT & allKeys){
			DMMLabel_Repaint();
		}
		if (GENERATOR_PANEL_REPAINT_BIT&allKeys) {
			vRepaint_geneeral_panel();
		}
		if (RUNMODE_PANEL_REPAINT_BIT&allKeys) {
			vRepaint_RUN_mode_panel();
		}
		if (CURSOR_PANEL_REPAINT_BIT &allKeys) {
			RepaintCursorPanel();
		}
		if (CURSOR_LABEL_REPAINT_BIT &allKeys) {
			RepaintCursorLabel();
		}
		if (SCALE_LABEL_REPAINT_BIT & allKeys) {
			RepaintScaleLabel();
		}
		if (SCALE_PANEL_REPAINT_BIT & allKeys) {
			vRepaint_SCALE_panel();
		}
		if( allKeys& WAVEFORM_PANEL_REPAINT_BIT)
		{
			RepaintWaveformPanel(); // 重绘波形面板
		}





	}
}




void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if(hspi->Instance == SPI1)
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;

		xSemaphoreGiveFromISR(xSemForDMa, &xHigherPriorityTaskWoken);

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}
static void prvShowImage(uint16_t* image)
{
	// 设置图像的显示范围
	// 列：40 ~ 199 => 0x28 ~ 0xC7（宽度 160）从80行道239行
    PrvSendCommand(0x2a);
   PrvSendData((uint8_t[]){0x00, 0x3C, 0x00, 0xB3}, 4);

    // 行：100 ~ 219 => 0x64 ~ 0xDB（高度 120）
    PrvSendCommand(0x2b);
     PrvSendData((uint8_t[]){0x00, 0x50, 0x00, 0xEF}, 4);
    PrvSendCommand(0x2C);
    PrvSendData((uint8_t *)image, 160 * 120 * 2);
}
