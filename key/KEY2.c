#include "KEY2.h"
#include "general_panel.h"
#include "key.h"
#include "Run_mode_panel.h"
#include "cursor_panel.h"
#include "scale_panel.h"
#include "stm32f1xx_hal_gpio.h"

key_Handle Key2;

GPIO_PinState key2ReadPin(void){
    return HAL_GPIO_ReadPin(KEY2PORT, KEY2PIN);
}

void key2Callback(void* args){
    (void)args;
    vSelected_SCALE_panel();
    vCancel_general_panel();
    vCancel_RUN_mode_panel();
    DeselectCursorPanel();
    // TODO: Key2 按下要执行的动作
}

void key2Scan(void){
    vkeyOneSca(&Key2);
}

void key2Init(void){
    Key2.rounte = ROUTE_QUEUE;
    Key2.xSem   = NULL;
    Key2.Init.arg = NULL;
    Key2.Init.ClickCallback = key2Callback;
    Key2.Init.keyReadPin    = key2ReadPin;
    keyInit(&Key2);
}