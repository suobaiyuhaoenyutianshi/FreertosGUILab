/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 取异常现场：测 EXC_RETURN 的 bit2 选 MSP/PSP，把帧指针(r0)和 EXC_RETURN(r1)
 * 作为参数尾调用 hardfault_dump。硬故障/总线/用法/内存管理 四个处理器共用。
 * hardfault_dump 里 while(1) 永不返回，所以用 b(尾调用)而不是 bl。 */
#define HF_DUMP_FRAME()                       \
    __asm volatile (                          \
        "tst lr, #4       \n"                 \
        "ite eq           \n"                 \
        "mrseq r0, msp    \n"                 \
        "mrsne r0, psp    \n"                 \
        "mov r1, lr       \n"                 \
        "b hardfault_dump \n"                 \
    )
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ============================ HardFault 现场抓取 ============================
 * Cortex-M3 进入异常时，硬件会自动把 8 个寄存器压进"当时正在用的那个栈"，
 * 顺序固定（低地址在前），所以可以直接用下标读：
 *
 *   stack[0] = R0    stack[1] = R1    stack[2] = R2    stack[3] = R3
 *   stack[4] = R12   stack[5] = LR    stack[6] = PC    stack[7] = xPSR
 *
 * 各字段怎么用（调试时按这个顺序看）：
 *   hf_pc         出错的那条指令地址 → 用 addr2line 或反汇编窗口换成"源文件:行号"
 *   hf_lr         出错时的返回地址   → 告诉你"是从哪个函数调进来的"
 *   hf_r0 ~ hf_r3 函数前 4 个参数    → 非法读写的那个坏地址常躺在里面
 *   hf_psr        低 9 位是异常号(IPSR)：0=线程 3=HardFault 16+n=第n个IRQ
 *   hf_exc_return 进异常时的 LR 特殊值：0xFFFFFFFD = 崩在任务里(用的是 PSP)
 *                                     0xFFFFFFF9 = 崩在中断里(用的是 MSP)
 *   hf_hfsr       FORCED 位=1 说明是别的 fault 升级上来的
 *   hf_cfsr       细分原因：PRECISERR / IMPRECISERR / UNALIGNED / DIVBYZERO / IBUSERR ...
 *   hf_bfar       出错时访问的数据地址(BFARVALID=1 时有效，寄存器 0xE000ED38)
 *   hf_mmfar      MemManage 故障的坏地址(CFSR 低 8 位非 0 时看它，不看 BFAR)
 *   hf_sp         出事时用的栈指针(MSP 或 PSP)，事后走栈/bt 要用
 *   hf_cur_task   出事时当前任务的 TCB 句柄；hf_task_name 是任务名(空=调度器没起来)
 * 注意：这些变量在 RAM 里，按暂停时用 Watch 窗口看即可，不要在 printf 里读(可能再次崩)。
 * ========================================================================== */
/* ============ 一次性抓全现场：打包成一个结构体 hf_ctx ============
 * 以前散装成 15 个 hf_* 全局变量，调试要一条条 `p/x hf_xxx` 看，慢。
 * 现在全部装进一个结构体，看一个容器就能拿到所有现场：
 *   - GDB 里        p hf_ctx         → 一行打印全部字段
 *   - Console 里    p/x hf_ctx.pc   → 单字段按十六进制看
 *   - Watch/LiveWatch 面板添加 hf_ctx → 点开三角逐字段展开
 * 同时保留旧变量名宏别名(hf_pc→hf_ctx.pc)，旧代码/注释引用不破。
 * ================================================================ */
typedef struct {
    uint32_t r0, r1, r2, r3;   /* 函数前 4 个参数，非法地址常躺在 r3 */
    uint32_t r12;              /* 临时变量 */
    uint32_t lr;               /* 返回地址 → 谁调用进来的 */
    uint32_t pc;               /* 现场指令 → 正在执行哪条 */
    uint32_t psr;              /* 状态/异常号(低 9 位 IPSR) */
    uint32_t exc_return;       /* EXC_RETURN：0xFFFFFFFD=任务(PSP)，0xFFFFFFF9=中断(MSP) */
    uint32_t hfsr;             /* FORCED 位=1 说明是下级 fault 升级来的 */
    uint32_t cfsr;             /* 细分原因：PRECISERR/IMPRECISERR/UNALIGNED/IBUSERR... */
    uint32_t bfar;             /* 出错访问的数据地址(BFARVALID=1 时有效，0xE000ED38) */
    uint32_t mmfar;            /* MemManage 坏地址(0xE000ED34，CFSR 低 8 位非 0 时看) */
    uint32_t sp;               /* 出事时用的栈指针(MSP/PSP)，事后走栈/bt 要用 */
    uint32_t cur_task;         /* 出事时当前任务的 TCB 句柄(0=调度器没起来) */
    const char *task_name;     /* 出事时当前任务名(指向 TCB 内字符串) */
} hf_ctx_t;

volatile hf_ctx_t hf_ctx;      /* 现场唯一容器，Watch 这一个就够了 */

/* 旧变量名保留为宏，指向 hf_ctx 对应字段，兼容外部引用 */
#define hf_r0        hf_ctx.r0
#define hf_r1        hf_ctx.r1
#define hf_r2        hf_ctx.r2
#define hf_r3        hf_ctx.r3
#define hf_r12       hf_ctx.r12
#define hf_lr        hf_ctx.lr
#define hf_pc        hf_ctx.pc
#define hf_psr       hf_ctx.psr
#define hf_exc_return hf_ctx.exc_return
#define hf_hfsr      hf_ctx.hfsr
#define hf_cfsr      hf_ctx.cfsr
#define hf_bfar      hf_ctx.bfar
#define hf_mmfar     hf_ctx.mmfar
#define hf_sp        hf_ctx.sp
#define hf_cur_task  hf_ctx.cur_task
#define hf_task_name hf_ctx.task_name

void hardfault_dump(uint32_t *stack, uint32_t exc_return)
{
    hf_ctx.r0     = stack[0];
    hf_ctx.r1     = stack[1];
    hf_ctx.r2     = stack[2];
    hf_ctx.r3     = stack[3];
    hf_ctx.r12    = stack[4];
    hf_ctx.lr     = stack[5];
    hf_ctx.pc     = stack[6];
    hf_ctx.psr    = stack[7];
    hf_ctx.sp     = (uint32_t)stack;     // 保存出事时的 MSP/PSP，事后走栈/bt 要用

    hf_ctx.exc_return = exc_return;      // 判断崩在任务(PSP)还是中断(MSP)
    hf_ctx.hfsr   = SCB->HFSR;           // 是否由其它 fault 升级而来
    hf_ctx.cfsr   = SCB->CFSR;           // 真正的故障原因
    hf_ctx.bfar   = SCB->BFAR;           // 出错访问的数据地址(0xE000ED38)
    hf_ctx.mmfar  = SCB->MMFAR;          // MemManage 坏地址(0xE000ED34，CFSR 低 8 位非 0 时看)

#ifdef BM_BAREMETAL
    // 裸机宿主没有 FreeRTOS 任务上下文
    hf_ctx.cur_task = 0;
    hf_ctx.task_name = NULL;
#else
    // 记下崩在哪个任务(FreeRTOS)：调度器没起来时句柄为 NULL，跳过，避免二次崩溃
    hf_ctx.cur_task = (uint32_t)xTaskGetCurrentTaskHandle();
    if (hf_ctx.cur_task != 0)
    {
        hf_ctx.task_name = pcTaskGetName((TaskHandle_t)hf_ctx.cur_task);
    }
    else
    {
        hf_ctx.task_name = NULL;
    }
#endif

    while (1);   /* 停在这里，Watch hf_ctx 即可一次看完全部现场 */
}
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_adc1;
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
 
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  HF_DUMP_FRAME();   /* 抓现场：选 MSP/PSP → 尾调用 hardfault_dump(宏见文件顶部) */
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  HF_DUMP_FRAME();   /* 默认未使能时会升级成 HardFault；一旦使能，这里也抓现场 */
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  HF_DUMP_FRAME();   /* 默认未使能时会升级成 HardFault；一旦使能，这里也抓现场 */
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  HF_DUMP_FRAME();   /* 默认未使能时会升级成 HardFault；一旦使能，这里也抓现场 */
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/* SVC_Handler 由 FreeRTOS 的 port.c 提供（FreeRTOSConfig.h 里用直接路由把
 * vPortSVCHandler 映射成 SVC_Handler）。这里不能再定义，否则链接报 multiple definition。 */

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/* PendSV_Handler 由 FreeRTOS 的 port.c 提供（xPortPendSVHandler 的别名），这里不再定义。 */

/* SysTick_Handler 由 FreeRTOS 的 port.c 提供（xPortSysTickHandler 的别名），这里不再定义。 */

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line3 interrupt.
  */
void EXTI3_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI3_IRQn 0 */

  /* USER CODE END EXTI3_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
  /* USER CODE BEGIN EXTI3_IRQn 1 */

  /* USER CODE END EXTI3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel1 global interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

  /* USER CODE END DMA1_Channel1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc1);
  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel3 global interrupt.
  */
void DMA1_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel3_IRQn 0 */

  /* USER CODE END DMA1_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi1_tx);
  /* USER CODE BEGIN DMA1_Channel3_IRQn 1 */

  /* USER CODE END DMA1_Channel3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel4 global interrupt.
  */
void DMA1_Channel4_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel4_IRQn 0 */

  /* USER CODE END DMA1_Channel4_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
  /* USER CODE BEGIN DMA1_Channel4_IRQn 1 */

  /* USER CODE END DMA1_Channel4_IRQn 1 */
}

/**
  * @brief This function handles ADC1 and ADC2 global interrupts.
  */
void ADC1_2_IRQHandler(void)
{
  /* USER CODE BEGIN ADC1_2_IRQn 0 */

  /* USER CODE END ADC1_2_IRQn 0 */
  HAL_ADC_IRQHandler(&hadc1);
  /* USER CODE BEGIN ADC1_2_IRQn 1 */

  /* USER CODE END ADC1_2_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
