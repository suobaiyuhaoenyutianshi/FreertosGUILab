#include "KEY4.h"
#include "key.h"
#include "Run_mode_panel.h"
#include "cursor_panel.h"
#include "scale_panel.h"
#include "general_panel.h"
#include "stm32f1xx_hal_gpio.h"
#include "Run_mode_panel.h"

key_Handle Key4;

GPIO_PinState key4ReadPin(void){
    return HAL_GPIO_ReadPin(KEY4PORT, KEY4PIN);
}

void key4Callback(void* args){
    (void)args;
    vCancel_RUN_mode_panel();
    DeselectCursorPanel();
    vCancel_SCALE_panel();
    vSelected_general_panel();
    // TODO: Key4 按下要执行的动作
}

void key4Scan(void){
    vkeyOneSca(&Key4);
}

void key4Init(void){
    Key4.rounte = ROUTE_QUEUE;
    Key4.xSem   = NULL;
    Key4.Init.arg = NULL;
    Key4.Init.ClickCallback = key4Callback;
    Key4.Init.keyReadPin    = key4ReadPin;
    keyInit(&Key4);
}