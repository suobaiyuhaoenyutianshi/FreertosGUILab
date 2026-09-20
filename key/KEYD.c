#include "KEYD.h"
#include "key.h"
#include "general_panel.h"
#include "scale_panel.h"
#include "stm32f1xx_hal_gpio.h"
#include "Run_mode_panel.h"
#include "cursor_panel.h"
key_Handle KeyD;

GPIO_PinState keyDReadPin(void){
    return HAL_GPIO_ReadPin(KEYD, KEYPIN);   // KEYD 是端口，KEYPIN 是引脚
}

void keyDCallback(void* args){
    (void)args;
    vadjust_general_panel(-1);
    vadjust_RUN_mode_panel(-1);
    vadjust_SCALE_panel(-1);
    AdjustCursorPanel(-1);
    // TODO: KeyD 按下要执行的动作
}

void keyDScan(void){
    vkeyOneSca(&KeyD);
}

void keyDInit(void){
    KeyD.rounte = ROUTE_QUEUE;
    KeyD.xSem   = NULL;
    KeyD.Init.arg = NULL;
    KeyD.Init.ClickCallback = keyDCallback;
    KeyD.Init.keyReadPin    = keyDReadPin;
    keyInit(&KeyD);
}