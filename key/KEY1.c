#include "KEY1.h"
#include "key.h"
#include "main.h"
#include "general_panel.h"
#include "scale_panel.h"

#include "cursor_panel.h"
#include "Run_mode_panel.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_gpio.h"
#include "Run_mode_panel.h"
key_Handle Key1;
GPIO_PinState key1ReadPin(void);
//队列数量
#define ucQuNum 4
//存放任务指针
QueueHandle_t xKey1Que;
void key1Scan(void){

    vkeyOneSca(&Key1);
}
GPIO_PinState key1ReadPin(void){
    return HAL_GPIO_ReadPin(KEY1PORT,KEY1PIN);
}
void key1Callback(void* args){
    (void)args;
    // HAL_GPIO_WritePin(BUZZER1_GPIO_Port,BUZZER1_Pin,GPIO_PIN_SET);
    // vTaskDelay(pdMS_TO_TICKS(10));
    // HAL_GPIO_WritePin(GPIOC,GPIO_PIN_8,GPIO_PIN_RESET);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    DeselectCursorPanel();
    vCancel_SCALE_panel();
    vCancel_general_panel();
    vSelected_RUN_mode_panel();
   // HAL_GPIO_WritePin()
}
void key1Init(void){
    Key1.rounte = ROUTE_QUEUE;
    Key1.xSem = NULL;
  Key1.Init.arg = NULL;
  Key1.rounte = ROUTE_QUEUE;
   Key1.Init.ClickCallback = key1Callback;
    Key1.Init.keyReadPin = key1ReadPin;
    keyInit(&Key1);
}


