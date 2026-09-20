#include "KEYU.h"
#include "key.h"
#include "Run_mode_panel.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_gpio.h"
#include "general_panel.h"
#include "scale_panel.h"
#include "cursor_panel.h"
key_Handle KeyU;
//SemaphoreHandle_t xKeyUSem;   // 暴露出来，别的任务可以直接 Take

GPIO_PinState keyUReadPin(void){
    return HAL_GPIO_ReadPin(KEYU, KEYUPIN);
}
void keyUCallback(void* args){
    (void)args;
     vadjust_general_panel(1);
    vadjust_RUN_mode_panel(1);
    vadjust_SCALE_panel(1);
    AdjustCursorPanel(1);
    // TODO: KeyD 按下要执行的动作
}
void keyUScan(void){
    vkeyOneSca(&KeyU);
}

void keyUInit(void){
    KeyU.rounte = ROUTE_QUEUE;
    KeyU.xSem   =NULL; //xSemaphoreCreateCounting(4,0);   // ← 创建本键自己的信号量
    //configASSERT(KeyU.xSem != NULL); 这些注释是是之前展示信号量的用法
    //xKeyUSem = KeyU.xSem;                     // 对外暴露

    KeyU.Init.arg = NULL;
    KeyU.Init.ClickCallback =keyUCallback ;           // 走信号量不需要回调
    KeyU.Init.keyReadPin    = keyUReadPin;
    keyInit(&KeyU);
}
//对应的任务
/*void vKeyUTask(void *pv){
    (void)pv;
    for(;;){
        if(xSemaphoreTake(xKeyUSem, portMAX_DELAY) == pdPASS){
            // KEYU 被按下了
        }
    }
}*/