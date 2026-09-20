#include "KEY3.h"
#include "key.h"
#include "general_panel.h"

#include "Run_mode_panel.h"
#include "cursor_panel.h"
#include "scale_panel.h"
#include "stm32f1xx_hal_gpio.h"

key_Handle Key3;

GPIO_PinState key3ReadPin(void){
    return HAL_GPIO_ReadPin(KEY3PORT, KEY3PIN);
}

void key3Callback(void* args){
    (void)args;
    SelectCursorPanel();
    vCancel_general_panel();
    vCancel_RUN_mode_panel();
    vCancel_SCALE_panel();
    // TODO: Key3 按下要执行的动作
}

void key3Scan(void){
    vkeyOneSca(&Key3);
}

void key3Init(void){
    Key3.rounte = ROUTE_QUEUE;
    Key3.xSem   = NULL;
    Key3.Init.arg = NULL;
    Key3.Init.ClickCallback = key3Callback;
    Key3.Init.keyReadPin    = key3ReadPin;
    keyInit(&Key3);
}