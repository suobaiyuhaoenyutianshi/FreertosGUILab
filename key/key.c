//任务写通用的检测按钮   与  初始化
#include "key.h"
#include "main.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f1xx_hal_gpio.h"
//记得在负责所有键检测 需要为创建的队列给大小  别忘了
QueueHandle_t xKeysQue;//这个创建的大小是job_t
//为什么还是单独一个
void vkeyOneSca(key_Handle* key){


        GPIO_PinState  cur = key->Init.keyReadPin();
    if((cur != key->pre) && cur == GPIO_PIN_RESET){
        job_t job;
        //队列的
       if(key->rounte == ROUTE_QUEUE){
           job.xfu = key->Init.ClickCallback;
            job.arg = key->Init.arg;//回调函数的参数，你也可以在回调里发信号
            xQueueSend(xKeysQue,&job,0);
       }else {
        xSemaphoreGive(key->xSem);
       }
     
    }



    key->pre = cur;
      
    


}
void keyInit(key_Handle* key){
    key->pre = GPIO_PIN_SET;
    
}

