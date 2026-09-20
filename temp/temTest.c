#include "temTest.h"
#include "main.h"
#include "app_usart1_dma.h"
#include "memory_monitor.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_def.h"
#include "timers.h"
#include <stdint.h>
void vTempTest_Init(void);
//定时器所调用的回调函数,定时调用
void vTempCall(TimerHandle_t xTimer);
//回调使得这个取得信号接着收集
void vTempPro(void* pVparamate);
//任务句柄
TaskHandle_t XtemP_t = NULL;
//定时器句柄
TimerHandle_t Xtem_tim = NULL;
volatile uint8_t tem =0;

void vTempTest_Init(void){
    tem++;
    xTaskCreate(vTempPro,"temTest",256,NULL,2,&XtemP_t);
    Xtem_tim = xTimerCreate("temp_tim",pdMS_TO_TICKS(100),pdTRUE,NULL,vTempCall);
    if (Xtem_tim !=NULL) {
        xTimerStart(Xtem_tim,0);
    }
    vRegisterTaskForMonitoring(XtemP_t);

}
void vTempCall(TimerHandle_t xTimer){
    (void) xTimer;
    if (XtemP_t!=NULL) {
        xTaskNotifyGive(XtemP_t);

    }




}
void vTempPro(void* pVparamate){
    (void)pVparamate;
    for (; ;) {
        // 【思路】死等通知。收到前任务处于挂起态，不消耗CPU。
        // pdTRUE: 收到通知后自动清零，相当于“二值信号量”。
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        extern ADC_HandleTypeDef hadc1;
        HAL_ADC_Start_IT(&hadc1);
        HAL_ADC_PollForConversion(&hadc1,100);
// 可以轮询，卡住只是卡这个任务等级较低 而且这不因该卡 因为短 ，轮询是因为刚开启需要时间
         float voltage = HAL_ADC_GetValue(&hadc1) / 4095.0f * 3.3f;
        float temperature = (voltage - 1.43f) / 0.0043f + 25.0f;

      //p_USART1_Printf("temperature = %.2fC\r\n", temperature);  // 注释掉这行
// 换成简单的整数打印，或者干脆不打印
uint32_t temp_int = (uint32_t)(temperature * 100);
App_USART1_Printf("temperature = %d.%02dC\r\n", temp_int/100, temp_int%100); // 避免浮点
       
    }


}