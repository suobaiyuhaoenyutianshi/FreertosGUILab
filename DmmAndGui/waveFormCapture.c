#include "FreeRTOS.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f103xe.h"
#include "LCD_TASK.h"
#include "waveFormCapture.h"
#include "waveform_panel.h"
#include "main.h"
#include "dmm.h"
#include <stdint.h>
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim3;
extern EventGroupHandle_t xEventLcd;

// 触发源：PA3 上升沿（EXTI3），强制触发也走这一条线
#define WAVE_TRIGGER_PIN   GPIO_PIN_3
#define WAVE_TRIGGER_IRQn  EXTI3_IRQn

// ── 双缓冲①：DMA 写一块、换算任务读另一块，中断里只切换索引，不需要锁 ──
static uint16_t fRawBuffer[2][1024]={0};   // ADC 原始码值
static volatile uint8_t ucWriteIdx = 0;    // DMA 正在写的缓冲号
static volatile uint8_t ucReadyIdx = 0xFF; // 已填满待换算的缓冲号（0xFF = 无）

// ── 双缓冲②：换算任务写一块、LCD 任务读另一块，改一次索引就完成"发布" ──
static float fWaveform[2][1024]={0};       // 换算后的电压值
static volatile uint8_t ucShowIdx = 0;     // 当前可显示的缓冲号

static TimerHandle_t xHoldOffTimer;        // Hold-Off：采集+换算期间禁止再次触发
static TimerHandle_t xForceTriggerTimer;   // 强制触发：没有外部边沿时也要刷新画面
static void vHoldOffTimerCallBack(TimerHandle_t timer);
static void vForceTriggerTimerCallBack(TimerHandle_t timer);

static TaskHandle_t XFloat_ari_t = NULL;
static void vFloating_arithmeticPro(void* pVparamate);
static void vUpdateTriggerTimers(void);

// @作用：按时基联动调整 Hold-Off / 强制触发两个软件定时器的周期
// 一帧采集时间 = 1024/fs = 1024×(Psc+1)×(Arr+1)/72MHz。约束：
//   ① Hold-Off 必须 > 采集时间 + 浮点换算(约4ms)，否则换算未完成就重装 DMA → 半帧
//   ② 强制触发周期必须 >= Hold-Off，否则慢挡位下强制触发会在采集途中再次重启 DMA
// 旧代码把两者写死为 20ms/100ms，只对中速挡位可用；慢挡位一帧时间超过它们就会出问题。
static void vUpdateTriggerTimers(void){
    // 采集时间，单位 µs；与 uWaveCaptureGetSampleRate 同样直接读寄存器
    uint32_t frameUs = (uint32_t)(1024.0f
        * (htim3.Instance->PSC + 1)
        * (htim3.Instance->ARR + 1)
        * 1000.0f / 72.0e6f);

    // Hold-Off 必须 > 采集时间 + 换算(约4ms)：保证整帧采完、换算完才允许下一次触发
    uint32_t holdMs = (frameUs / 1000u) + 6u;   // 采集 + 换算约4ms + 保护余量2ms
    if (holdMs < 10u) holdMs = 10u;             // 下限保护，避免过快刷新

    // 强制触发必须晚于 Hold-Off 结束：Hold-Off 先 CLEAR_FLAG + EnableIRQ，
    // 强制触发再写 SWIER 才不会被那个 CLEAR_FLAG 清掉。两者相等会有竞态 → 偶尔丢一帧。
    uint32_t forceMs = holdMs + 5u;

    xTimerChangePeriod(xHoldOffTimer,      pdMS_TO_TICKS(holdMs),  pdMS_TO_TICKS(100));  // 末参=命令队列满时最多等100ms再发
    xTimerChangePeriod(xForceTriggerTimer, pdMS_TO_TICKS(forceMs), pdMS_TO_TICKS(100));
}

// @作用：初始化波形捕捉器
void WaveCaptureInit(void){
    HAL_TIM_Base_Start(&htim3);
    // Hold-Off 周期由 vUpdateTriggerTimers() 随时基动态算出，此处只创建拿到句柄
    xHoldOffTimer = xTimerCreate("HOLDoff",
        pdMS_TO_TICKS(20),
        pdFALSE,
        NULL,
        vHoldOffTimerCallBack);
    xForceTriggerTimer = xTimerCreate("forceTriger",
        pdMS_TO_TICKS(100),
        pdTRUE,NULL,
        vForceTriggerTimerCallBack);
    //创建float点数计算的任务（栈深度由头文件宏统一定义，监控注册时复用同一个值）
    xTaskCreate(vFloating_arithmeticPro,"float_archIT",WAVE_CAPTURE_TASK_STACK_WORDS,NULL,2,&XFloat_ari_t);
    //开启强制触发（没有外部边沿时也能定期刷新）
    xTimerStart(xForceTriggerTimer,0);
    // 按当前 TIM3 时基把两个定时器周期校准到正确值
    vUpdateTriggerTimers();
}

// @作用：获取可显示的波形（指向已发布的那一块，读到的永远是完整一帧）
float * waveDate(void){
    return fWaveform[ucShowIdx];
}

// @作用：把浮点换算任务的句柄交出去，供内存监控注册（任务本身仍是本文件私有）
TaskHandle_t xWaveCaptureGetTaskHandle(void){
    return XFloat_ari_t;
}

// @作用：设置波形捕捉器的采样率
// @参数：Psc、Arr - TIM3 的预分频与自动重装值，fs = 72MHz/(Psc+1)/(Arr+1)
void WaveCaptureSetSampleRate(uint16_t Psc, uint16_t Arr){
    HAL_TIM_Base_Stop(&htim3);
    __HAL_TIM_SET_PRESCALER(&htim3, Psc);
    __HAL_TIM_SET_AUTORELOAD(&htim3, Arr);
    HAL_TIM_Base_Start(&htim3);
    
    // 时基变了，Hold-Off / 强制触发周期必须跟上，否则慢挡位会采半帧或被中途重启
    vUpdateTriggerTimers();
}

// @作用：获取当前采样率，单位 Hz
uint32_t uWaveCaptureGetSampleRate(void){
    return (uint32_t)(72.0e6f/(htim3.Instance->PSC+1)/(htim3.Instance->ARR+1));
}

// @作用：EXTI 触发中断（外部上升沿，或强制触发定时器写 SWIER 造成的"假边沿"）
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    
    if (GPIO_Pin == WAVE_TRIGGER_PIN) {
            BaseType_t xhigWoken = pdFALSE;
        // 中断里绝不用锁：DMA 往 fRawBuffer[ucWriteIdx] 写，换算任务读 fRawBuffer[ucReadyIdx]
            HAL_ADC_Start_DMA(&hadc1,(uint32_t*)fRawBuffer[ucWriteIdx],1024);//设置是半字传输
            // 屏蔽中断，以防再次触发
            HAL_NVIC_DisableIRQ(WAVE_TRIGGER_IRQn);
            xTimerStartFromISR(xHoldOffTimer,&xhigWoken);
            // 复位强制触发定时器
            xTimerResetFromISR(xForceTriggerTimer,&xhigWoken);
            portYIELD_FROM_ISR(xhigWoken);
   }

}
//闭Ex中断使能的函数// 禁止触发定时器的回调函数
static void vHoldOffTimerCallBack(TimerHandle_t timer){
    (void)timer;
    	// 清除中断标志
    __HAL_GPIO_EXTI_CLEAR_FLAG(WAVE_TRIGGER_PIN);
    HAL_NVIC_ClearPendingIRQ(WAVE_TRIGGER_IRQn);
     //再次开启中断，禁止触发结束
	HAL_NVIC_EnableIRQ(WAVE_TRIGGER_IRQn);


}//强制触发Ex中断
static void vForceTriggerTimerCallBack(TimerHandle_t timer){
    (void)timer;

    __HAL_GPIO_EXTI_GENERATE_SWIT(WAVE_TRIGGER_PIN);



}

// @作用：ADC 常规序列（1024 点 DMA）采集完成
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
      BaseType_t xhigWoken= pdFALSE;
    if (hadc->Instance ==ADC1) {
        
            ucReadyIdx = ucWriteIdx;    // 这一块填满了，交给换算任务
            ucWriteIdx ^= 1;            // 下一帧写另一块
            vTaskNotifyGiveFromISR(XFloat_ari_t,&xhigWoken);
            portYIELD_FROM_ISR(xhigWoken);
    }
}

// @作用：把原始码值换算成电压（放在任务里做，不占用中断）
static void vFloating_arithmeticPro(void* pVparamate){
    (void)pVparamate;
    for(;;){
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

        uint8_t ucReadIdx = ucReadyIdx;
        if (ucReadIdx == 0xFF) {
            continue;   // 没有被标记的帧，等下一次
        }
        ucReadyIdx = 0xFF;

        uint8_t ucDstIdx = ucShowIdx ^ 1;   // 写到"当前没在显示"的那一块
        for(int i=0; i<1024; i++)
        {
            // 与 dmm.c 保持一致：先用 VDDA 还原 ADC 引脚电压，再按前端(±2V 偏置)反算输入电压
            fWaveform[ucDstIdx][i] = (fRawBuffer[ucReadIdx][i] / 4095.0f * fGetVdda()) * 2.0f - 2.0f;
        }
        ucShowIdx = ucDstIdx;   // 单字节写 = 原子"发布"，LCD 读到的永远是完整一帧
        xEventGroupSetBits(xEventLcd, WAVEFORM_PANEL_REPAINT_BIT);
    }
}
