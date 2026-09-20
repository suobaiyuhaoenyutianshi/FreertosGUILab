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
void vKeyWOrksScan(void* pvArg);//检测任务



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
    
  
/*//是检测任务循环前的调用每个键的初始化  与创建消费者任务 队列QueueHandle_t xKeysQue 这个创建的大小是job_t 10个先*/
void VkeyTotalInit(void* pvArg){
    (void) pvArg;
    xKeysQue = xQueueCreate(10,sizeof(job_t));
    configASSERT(xKeysQue != NULL);

    key1Init();
    key2Init();
    key3Init();
    key4Init();
    keyUInit();
    keyDInit();
    
}
//检测任务
void vKeyWOrksScan(void* pvArg){
    (void)pvArg;
    VkeyTotalInit(NULL);
    for(;;){
      key1Scan();
        key2Scan();
        key3Scan();
        key4Scan();
        keyUScan();
        keyDScan();

        vTaskDelay(pdMS_TO_TICKS(10));

    }



}