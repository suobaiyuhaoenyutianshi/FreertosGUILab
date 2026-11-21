//任务写通用的检测按钮   与  初始化
#include "key.h"
#include "main.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32_hal_legacy.h"
#include "stm32f1xx_hal_gpio.h"
//记得在负责所有键检测 需要为创建的队列给大小  别忘了
QueueHandle_t xKeysQue;//这个创建的大小是job_t
/* 慢回调专用队列：KEYU/KEYD 的按下事件走这里，由 KEYSLOW 任务串行执行 */
QueueHandle_t xSlowKeyQue;

#ifdef BM_BAREMETAL
// 裸机宿主：不建 FreeRTOS 队列，用一个很小的静态环形队列承接按键事件
#define BM_KEYQ_LEN 8
static job_t s_bmKeyQ[BM_KEYQ_LEN];
static volatile uint8_t s_bmKeyHead = 0;
static volatile uint8_t s_bmKeyTail = 0;

static void bmKeyPush(job_t job){
    uint8_t next = (uint8_t)((s_bmKeyHead + 1u) % BM_KEYQ_LEN);
    if(next == s_bmKeyTail) return; // 满则丢，不阻塞
    s_bmKeyQ[s_bmKeyHead] = job;
    s_bmKeyHead = next;
}

uint8_t bmKeyPop(job_t *job){
    if(s_bmKeyTail == s_bmKeyHead) return 0;
    *job = s_bmKeyQ[s_bmKeyTail];
    s_bmKeyTail = (uint8_t)((s_bmKeyTail + 1u) % BM_KEYQ_LEN);
    return 1;
}
#endif
//为什么还是单独一个
void vkeyOneSca(key_Handle* key){
    GPIO_PinState  cur = key->Init.keyReadPin();
    if((cur != key->pre) && cur == GPIO_PIN_RESET){
        job_t job;
        job.xfu = key->Init.ClickCallback;
        job.arg = key->Init.arg;

#ifdef BM_BAREMETAL
        // 裸机宿主：统一进静态环形队列，由 main 循环取走/打印
        bmKeyPush(job);
#else
       if(key->rounte == ROUTE_QUEUE){
            //回调函数的参数，你也可以在回调里发信号
            xQueueSend(xKeysQue,&job,0);
       }else if(key->rounte == ROUTE_SLOW){
            xQueueSend(xSlowKeyQue,&job,0);
       }else {
        xSemaphoreGive(key->xSem);
       }
#endif
     
    }



    key->pre = cur;
      
    


}
void keyInit(key_Handle* key){
    key->pre = GPIO_PIN_SET;
    
}

