#include "dmm.h"
#include "main.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f103xe.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_adc_ex.h"
#include "stm32f1xx_hal_tim.h"
#include <stdint.h>
#include "LCD_TASK.h"
#include "FreeRTOS.h"
#include "event_groups.h"
static QueueHandle_t  xMailbox; // 邮箱句柄，静态全局变量，仅文件内部可见
static float fVddaNow = 3.3f; // 最近一次由 VREFINT 反推出的 VDDA，供其它模块换算共用

// 域 A 边界：ISR 只写快照 + 序号，BmCore 在任务里消费；seq 用于判断有没有新帧
static volatile uint16_t s_dmmRaw[4] = {0};
static volatile uint32_t s_dmmSeq = 0;
static uint32_t s_dmmSeqSeen = 0;

extern EventGroupHandle_t xEventLcd;

static DMMRange_t DMM_GetRange(void);
static float prvCalcDMMValue(float fVadc, DMMRange_t eRange);
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;


void vDMMInit(void){
    xMailbox = xQueueCreate(1,sizeof(DMMData_t));//邮箱

    // 打开 DWT 周期计数器，供 ISR/BmCore 统计耗时
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    HAL_TIM_Base_Start(&htim2);
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADCEx_InjectedStart_IT(&hadc1);

}
DMMData_t xDMMGetData(void){
    DMMData_t DMM;
    xQueuePeek(xMailbox,&DMM,portMAX_DELAY);
    return DMM;
}

//
// @作用：获取最近一次由 VREFINT 反推出的 VDDA
// @返回：VDDA，单位 V。上电到第一次注入采样完成前返回 3.3
//
float fGetVdda(void){
    return fVddaNow;
}

//
// @作用：获取万用表的当前挡位
// @返回：挡位值。eDMMRange2V - 2V电压挡 eDMMRange10V - 10V电压挡 eDMMRange50V - 50V电压挡
//           eDMMRange1kOhm - 1k电阻挡 eDMMRange10kOhm - 10k电阻挡 eDMMRange100kOhm - 100k电阻挡
//           eDMMRange1MOhm - 1M电阻挡
//
static DMMRange_t DMM_GetRange(void)
{
	DMMRange_t range = eDMMRange2V;

	if(HAL_GPIO_ReadPin(DMM50V_GPIO_Port, DMM50V_Pin) == GPIO_PIN_SET)
	{
		range = eDMMRange50V; // 50V
	}
	else if(HAL_GPIO_ReadPin(DMM10V_GPIO_Port, DMM10V_Pin) == GPIO_PIN_SET)
	{
		range = eDMMRange10V; // 10V
	}
	else if(HAL_GPIO_ReadPin(DMM2V_GPIO_Port, DMM2V_Pin) == GPIO_PIN_SET)
	{
		range = eDMMRange2V; // 2V
	}
	else if(HAL_GPIO_ReadPin(DMM10kOhm_GPIO_Port, DMM10kOhm_Pin) == GPIO_PIN_SET)
	{
		range = eDMMRange10kOhm; // 10kOhm
	}
	else if(HAL_GPIO_ReadPin(DMM100kOhm_GPIO_Port, DMM100kOhm_Pin) == GPIO_PIN_SET)
	{
		range = eDMMRange100kOhm; // 100kOhm
	}
	else if(HAL_GPIO_ReadPin(DMM1MOhm_GPIO_Port, DMM1MOhm_Pin) == GPIO_PIN_SET)
	{
		range = eDMMRange1MOhm; // 1MOhm
	}
	else
	{
		range = eDMMRange1kOhm; // 1kOhm
	}

	return range;
}

//
// @作用：根据ADC的结果和万用表的挡位计算最终结果
//
static float prvCalcDMMValue(float fVadc, DMMRange_t eRange)
{
	float result;

	float fVin = fVadc * 2.0f - 2.0f;

	if(eRange == eDMMRange2V) // 电压档2V
	{
		result = fVin * 1.0f;
	}
	else if(eRange == eDMMRange10V) // 电压档10V
	{
		result = fVin * 5.0f;
	}
	else if(eRange == eDMMRange50V) // 电压档50V
	{
		result = fVin * 25.0f;
	}
	else // 以下为电阻档
	{
		float R0, R1, Rx;

		if(eRange == eDMMRange1kOhm) // 电阻档1k
		{
			R0 = 200.0f;
		}
		else if(eRange == eDMMRange10kOhm) // 电阻档10k
		{
			R0 = 2000.0f;
		}
		else if(eRange == eDMMRange100kOhm) // 电阻档100k
		{
			R0 = 2.0e4f;
		}
		else  // 电阻档1M
		{
			R0 = 2.0e5f;
		}

		R1 = 2.0e6f;
		Rx = fVin / (2.5 - fVin) * R0;

		result = R1 * Rx / (R1 - Rx);
	}

	return result;
}
// @作用：BmCore 每 10 ms 调用一次，把 ISR 采集到的 raw 快照算成 DMM/电源/触发电平
// @说明：单写者（ISR）+ 序号快照；这里在任务上下文写邮箱和事件组，不再占用中断
void vDMMProcess(void){
	uint32_t seq1, seq2;
	uint16_t raw[4];

	// ISR 写 raw 后 seq++；任务读的过程中如果被 ISR 抢占，序号会变化，重读一次
	do {
		seq1 = s_dmmSeq;
		raw[0] = s_dmmRaw[0];
		raw[1] = s_dmmRaw[1];
		raw[2] = s_dmmRaw[2];
		raw[3] = s_dmmRaw[3];
		seq2 = s_dmmSeq;
	} while(seq1 != seq2);

	if(seq1 == s_dmmSeqSeen){
		return; // 没有新的一帧
	}
	s_dmmSeqSeen = seq1;

	if(raw[1] == 0){
		return; // VREFINT 还没准备好，等下一帧
	}

	fVddaNow = 4095.0f / raw[1] * 1.205f;

	// 万用表模块的运算放大电路的输出电压
	float fVdmm_adc = raw[0] / 4095.0f * fVddaNow;

	// 稳压电源模块的输出电压
	float fVpwr = raw[2] / 4095.0f * fVddaNow * 6.0f;

	DMMRange_t eDMMRange = DMM_GetRange(); // 获取当前挡位
	float fDMMValue = prvCalcDMMValue(fVdmm_adc, eDMMRange); // 计算结果

	// 触发电平：PA0 电位器（ADC1_IN0）供外部比较器作参考，同时由注入组
	// 第 4 路读回，用于屏幕画触发电平参考线。换算与波形通道保持一致。
	float fTriggerLevel = raw[3] / 4095.0f * fVddaNow * 2.0f - 2.0f;

	DMMData_t xDmmData;
	xDmmData.eDMMRange = eDMMRange;
	xDmmData.fDMMValue = fDMMValue;
	xDmmData.fVpwr = fVpwr;
	xDmmData.fTriggerLevel = fTriggerLevel;

	xQueueOverwrite(xMailbox, &xDmmData);
	xEventGroupSetBits(xEventLcd, PWR_LABLE_REPAINT_BIT | DMM_LABLE_REPAINT_BIT);
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc){
	if(hadc->Instance==ADC1){
		// ISR 只做 4 次寄存器读取 + 序号递增；浮点计算搬到 BmCore
		s_dmmRaw[0] = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
		s_dmmRaw[1] = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
		s_dmmRaw[2] = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
		s_dmmRaw[3] = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_4);
		s_dmmSeq++;

		__HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_JEOC);
		__HAL_ADC_ENABLE_IT(hadc, ADC_IT_JEOC);
	}
}