/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"          // 必须是最先被包含的 FreeRTOS 头文件
#include "projdefs.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_spi.h"
#include "task.h"              // 如果需要任务
#include "timers.h"            // 然后才是其他组件
#include "event_groups.h"      // 然后才是其他组件
#include "queue.h"
#include "memory_monitor.h"
#include "app_usart1_dma.h"
#include "dmm.h"
#include "waveFormCapture.h"
#include "waveform_generator.h"
#include <string.h>
#include "keyTask.h"
#include "LCD_TASK.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED1port GPIOC
#define LED3port GPIOC
#define led1PIN  GPIO_PIN_1
#define led3PIN  GPIO_PIN_3

/* 各任务栈深度（单位：字）。
 * 创建任务与内存监控注册必须用同一个值：监控要算"剩余百分比"，
 * 靠的就是申请栈深度；两处各写一个数字，改一处忘另一处，百分比就失真了。 */
#define TASK_STACK_KEY_SCAN     128
#define TASK_STACK_KEY_WORK     128
#define TASK_STACK_LED1         128
#define TASK_STACK_LCD          256
/* UART1 守门员任务由 App_USART1_Init() 内部创建，栈深度固定 256（见 app_usart1_dma.c） */
#define TASK_STACK_UART1_TX     256
/* 浮点换算任务的深度由 DmmAndGui/waveFormCapture.h 里的 WAVE_CAPTURE_TASK_STACK_WORDS 统一定义 */

/* 启动版本行：串口 115200 输出的第一行，用于确认烧进去的是哪次构建 */
#define FW_VERSION  "0.1.0"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac;

IWDG_HandleTypeDef hiwdg;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM5_Init(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 读 RCC->CSR 的复位标志。标志是粘滞的，调用方读完要 __HAL_RCC_CLEAR_RESET_FLAGS()，
 * 否则下一次复位会把上次的原因一起带出来。 */
static const char *pcResetCause(void)
{
    uint32_t csr = RCC->CSR;
    if (csr & RCC_CSR_IWDGRSTF) return "IWDG";
    if (csr & RCC_CSR_WWDGRSTF) return "WWDG";
    if (csr & RCC_CSR_LPWRRSTF) return "LPWR";
    if (csr & RCC_CSR_SFTRSTF)  return "SOFT";
    if (csr & RCC_CSR_PORRSTF)  return "POR";
    if (csr & RCC_CSR_PINRSTF)  return "PIN";
    return "UNKNOWN";
}

/* ====================== 【调试练习】故意制造一次 HardFault ======================
 * 目的：练习读 stm32f1xx_it.c 里 hardfault_dump() 抓到的那一堆寄存器。
 *
 * 玩法：
 *   1) 保持下面宏 = 1，编译烧写。上电约 1 秒后程序会停死（LED 不再闪）。
 *   2) 调试器里按"暂停"，你会停在 hardfault_dump() 的 while(1) 上（看调用栈就知道）。
 *   3) 打开 Watch / Live Watch，逐个看这几个全局变量：
 *        hf_pc         → 出错的那条指令地址（最关键）
 *        hf_lr         → 出错函数的返回地址，即"谁调用了它"
 *        hf_r0 ~ hf_r3 → 里面有一个等于刚才那个"坏地址"
 *        hf_psr        → 低 9 位是异常号
 *        hf_exc_return → 0xFFFFFFFD 表示崩在任务里（读的是 PSP）
 *        hf_cfsr       → 告诉你"为什么崩"（PRECISERR / UNALIGNED / ...）
 *        hf_bfar       → 出错时想访问的那个地址
 *   4) 拿 hf_pc 去查源文件行号：
 *        arm-none-eabi-addr2line -e build/Debug/FreertosGUILab.elf -f -C 0x08xxxxxx
 *      （或在 IDE 反汇编窗口 Ctrl+G 跳到该地址）
 *   5) 定位到下面那一行之后，把宏改成 0，重新编译即可恢复正常。
 *
 * 提示：先自己想，别急着往下看注释里写的答案。
 * ============================================================================ */
#define EXERCISE_CRASH_ENABLE   0

#if EXERCISE_CRASH_ENABLE
static void vExercise_Crash(void)
{
    /* 0x30000000 在 STM32F103 的存储映射里没有对应外设/内存 → 写它必然触发
     * BusFault。实测为"不精确错误(IMPRECISERR, CFSR=0x400)"：写缓冲把错误延迟
     * 上报，硬件来不及填 BFAR(BFARVALID=0)。找真凶：反汇编 hf_pc 附近的那条
     * STR，R3 里就是 0x30000000。 */
    volatile uint32_t *pBad = (volatile uint32_t *)0x30000000U;
    *pBad = 0x12345678U;     /* ← 出事的就是这一行 */
}
#endif

/* ==================== 【调试练习2】调用非法函数指针 → UsageFault ====================
 * 目的：和练习1对比——这次不是“数据/总线错误”，而是“用法错误(UsageFault)”。
 * 玩法：
 *   1) 保持 EXERCISE_JUMP_ENABLE=1，编译烧写。约 1 秒后程序停死（LED 不再闪）。
 *   2) 按暂停停在硬故障现场，Watch 看这几个：
 *        hf_psr        崩前 xPSR：看 bit24 T 位（0=非Thumb）；IPSR 是崩前异常号(线程=0)
 *        hf_cfsr       0x20000 = bit17 INVSTATE（试图进入非 Thumb 状态执行）
 *        hf_pc         → 反汇编/addr2line 看是哪次跳转闸的
 *        hf_lr         → 调用它的 vled1Task
 *   3) 关键区别点：这次 R3/PC 都像“正常地址”，靠 CFSR 的 INVSTATE(0x20000) + PC 偶数(bit0=0)
 *      才能认出是“用法错误”，不是去查“坏地址”。练的就是这个区分能力。
 * 提示：SHCSR 开了 USGFAULTENA 才能让 UsageFault 亲自接管（否则会升级成 HardFault，
 *      调试器寄存器视图里的当前 IPSR 就从 6 变回 3）。调试完把 EXERCISE_JUMP_ENABLE 改回 0。
 * ================================================================================= */
#define EXERCISE_JUMP_ENABLE   0

#if EXERCISE_JUMP_ENABLE
static void vExercise_JumpBad(void)
{
    /* 让 UsageFault 真正接管，这样 hf_psr 的 IPSR 才显示 6（否则升级成 HardFault=3） */
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;

    /* 把一个“偶数地址(bit0=0,即非 Thumb)”当作函数调用。
     * Cortex-M3 只认 Thumb(bit0=1)，遇到 bit0=0 就报 INVSTATE。 */
    void (*pBad)(void) = (void (*)(void))0x20004e00U;
    pBad();
}
#endif

void vled1Task(void* pvparameters){
  (void)pvparameters;
  for(;;){
#if EXERCISE_CRASH_ENABLE
    /* 先延时 1 秒再崩，给你时间连上调试器 / 看清 LED 闪了一下才停 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    vExercise_Crash();
#endif
#if EXERCISE_JUMP_ENABLE
    /* 先延时 1 秒再崩，给你时间连调试器 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    vExercise_JumpBad();
#endif
    HAL_GPIO_WritePin(LED1port,led1PIN,GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(500));
    HAL_GPIO_WritePin(LED1port,led1PIN,GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
void vled3Task(void* pvparameters){
  (void)pvparameters;
  for(;;){
   
    HAL_GPIO_WritePin(LED3port,led3PIN,GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(500));
    HAL_GPIO_WritePin(LED3port,led3PIN,GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

//
// @作用：控制屏幕背光的点亮和熄灭
// @参数：On - 0熄灭，非0点亮
//

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  TaskHandle_t LED1 ;
  TaskHandle_t KEYSWORK;
  TaskHandle_t KEYSSCAN;
  TaskHandle_t LCD;
  TaskHandle_t KEYSLOW;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_DAC_Init();
  MX_TIM5_Init();
  /* IWDG 暂不启用：CubeMX 6.18 重新生成时自动勾上了看门狗，但全工程没有喂狗点，
   * 0.41s 就会复位一次，表现为 LCD 花屏 + 闪烁。若以后要用，需配套 HAL_IWDG_Refresh()。 */
  //MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  App_USART1_Init();
  /* 启动版本行：上电后串口第一行，先取复位原因再清标志 */
  {
    const char *pcReset = pcResetCause();
    __HAL_RCC_CLEAR_RESET_FLAGS();
    App_USART1_Printf("fw ver %s | build %s %s | reset %s\r\n",
                      FW_VERSION, __DATE__, __TIME__, pcReset);
  }
   vLCDEventGroupInit(); // 在调度器启动前静态创建 xEventLcd，避免 ADC 注入 ISR 提前使用 NULL
  vDMMInit();
 
  WaveCaptureInit(); // 波形捕捉器：互斥锁、三个软件定时器、浮点换算任务
  vWaveGenInit();    // 信号发生器：DAC + TIM5 + DMA2
 
 xTaskCreate(vKeyWOrksScan,"KEYINITSCAN",TASK_STACK_KEY_SCAN,NULL,2,&KEYSSCAN);//只是扫描了key1 都小些可y1
  xTaskCreate(vKeyWorkerTask,"KEYIWORK",TASK_STACK_KEY_WORK,NULL,1,&KEYSWORK);//只有key1
  /* 慢按键队列 + 唯一执行者：KEYU/KEYD 共用。
   * 慢回调（重算波表、重启 DAC/TIM5、改时基）不占公共队列，也不会并发改硬件。
   * 队列必须在任务启动前建好，否则 KEYSLOW 第一次 xQueueReceive 就是空句柄。 */
  xSlowKeyQue = xQueueCreate(10,sizeof(job_t));
  configASSERT(xSlowKeyQue != NULL);
  xTaskCreate(vKeySlowTask,"KEYSLOW",TASK_STACK_KEY_SLOW,NULL,1,&KEYSLOW);
  xTaskCreate(vled1Task,"led1",TASK_STACK_LED1,NULL,1,&LED1);
  xTaskCreate(vLCDTask, "LCD", TASK_STACK_LCD, NULL, 1, &LCD); // 注意这里的任务栈大小设置为256
   //xTaskCreate(vDMMInit, "DMMIN", 256, NULL, 1, &LCD); // 注意这里的任务栈大小设置为256
   
  /* 注册内存监控名单。
   * 之前只有 LED1/LCD/串口/两个按键任务这 5 个"轻任务"，真正吃栈的两项反而漏了：
   *   - float_archIT：每帧对 1024 点做浮点换算，是示波器最重的业务任务；
   *   - KEYSLOW：KEYU/KEYD 慢回调的唯一执行者。
   * 注册时连同申请栈深度一起报进去，串口输出才能给出"剩余/总量 + 百分比"。 */
  vRegisterTaskForMonitoring(LED1,     TASK_STACK_LED1);//vDMMInit
  vRegisterTaskForMonitoring(LCD,      TASK_STACK_LCD);
  vRegisterTaskForMonitoring(usart1,   TASK_STACK_UART1_TX);
  vRegisterTaskForMonitoring(KEYSSCAN, TASK_STACK_KEY_SCAN);
  vRegisterTaskForMonitoring(KEYSWORK, TASK_STACK_KEY_WORK);
  vRegisterTaskForMonitoring(KEYSLOW, TASK_STACK_KEY_SLOW);
  /* WaveCaptureInit() 在上面的初始化区就已创建该任务，这里取句柄注册 */
  vRegisterTaskForMonitoring(xWaveCaptureGetTaskHandle(), WAVE_CAPTURE_TASK_STACK_WORDS);
  

  vStartMemoryMonitor();
  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_InjectionConfTypeDef sConfigInjected = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_9;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
  sConfigInjected.InjectedNbrOfConversion = 3;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfigInjected.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJECCONV_T2_TRGO;
  sConfigInjected.AutoInjectedConv = DISABLE;
  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
  sConfigInjected.InjectedOffset = 0;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_VREFINT;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_2;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_8;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_3;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T5_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_DISABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
__attribute__((unused)) static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 563;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 71;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 1;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, led1_Pin|ld3_Pin|BUZZER1_Pin|LCD_RS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_NSS_GPIO_Port, LCD_NSS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, lcdRtS_Pin|LCD_BL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : KEYU_Pin KEYD_Pin KEY3_Pin KEY4_Pin
                           KEY1_Pin */
  GPIO_InitStruct.Pin = KEYU_Pin|KEYD_Pin|KEY3_Pin|KEY4_Pin
                          |KEY1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : led1_Pin ld3_Pin BUZZER1_Pin LCD_RS_Pin */
  GPIO_InitStruct.Pin = led1_Pin|ld3_Pin|BUZZER1_Pin|LCD_RS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DMM50V_Pin DMM10V_Pin DMM2V_Pin DMM10kOhm_Pin */
  GPIO_InitStruct.Pin = DMM50V_Pin|DMM10V_Pin|DMM2V_Pin|DMM10kOhm_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : DMM100kOhm_Pin DMM1MOhm_Pin */
  GPIO_InitStruct.Pin = DMM100kOhm_Pin|DMM1MOhm_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY2_Pin */
  GPIO_InitStruct.Pin = KEY2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_NSS_Pin */
  GPIO_InitStruct.Pin = LCD_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_NSS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : lcdRtS_Pin LCD_BL_Pin */
  GPIO_InitStruct.Pin = lcdRtS_Pin|LCD_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* 在这里处理栈溢出错误，例如打印任务名、死循环或复位 */
    (void)xTask;          /* 避免未使用参数警告 */
    (void)pcTaskName;

    /* 简单示例：死循环等待调试器介入 */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
