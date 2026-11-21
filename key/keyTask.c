#include "keyTask.h"
#include "KEY1.h"
#include "key.h"
#include "main.h"
#include "projdefs.h"
#include "stm32f1xx_hal_def.h"

#include "KEY2.h"
#include "KEY3.h"
#include "KEY4.h"
#include "KEYU.h"
#include "KEYD.h"

void vKeyWorkerTask(void *pvArg);//消费者任务 队列
void VkeyTotalInit(void* pvArg);//是检测任务循环前的调用每个键的初始化  与创建消费者任务 队列QueueHandle_t xKeysQue 这个创建的大小是job_t 10个先
void vKeyScanStep(void);//单次扫描，由 BmCore 的 10 ms 分频调用



//消费者任务 队列
void vKeyWorkerTask(void *pvArg){
    (void)pvArg;
    job_t job;
    for(;;){
        if(xQueueReceive(xKeysQue,&job,portMAX_DELAY) == pdPASS){
          
            if (job.xfu != NULL) {
                job.xfu(job.arg);
            }
           // vTaskDelay(pdMS_TO_TICKS(13));
        }

    }


}

/* 慢回调的专用消费者：KEYU/KEYD 共用。
 * 与 vKeyWorkerTask 的唯一区别是它消费的是 xSlowKeyQue，所以重活（重算波表、
 * 重启 DAC/TIM5、改时基）不会堵住公共队列；同时只有一个执行者，天然串行。 */
void vKeySlowTask(void *pvArg){
    (void)pvArg;
    job_t job;
    for(;;){
        if(xQueueReceive(xSlowKeyQue,&job,portMAX_DELAY) == pdPASS){
            if (job.xfu != NULL) {
                job.xfu(job.arg);
            }
        }
    }
}
    
  
/*//是检测任务循环前的调用每个键的初始化  与创建消费者任务 队列QueueHandle_t xKeysQue 这个创建的大小是job_t 10个先*/
void VkeyTotalInit(void* pvArg){
    (void) pvArg;
    xKeysQue = xQueueCreate(10,sizeof(job_t)); // 队列项 = job_t(8字节)，与发送/接收端一致
    configASSERT(xKeysQue != NULL);

    key1Init();
    key2Init();
    key3Init();
    key4Init();
    keyUInit();
    keyDInit();
    
}
// 单次扫描：原来在 KEYSCAN 任务里每 10 ms 做一次，现在由 BmCore 的 10 ms 分频调用
void vKeyScanStep(void){
    key1Scan();
    key2Scan();
    key3Scan();
    key4Scan();
    keyUScan();
    keyDScan();
}