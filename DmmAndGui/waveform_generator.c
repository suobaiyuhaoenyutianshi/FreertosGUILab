/*
 * waveform_generator.c —— 信号发生器（驱动层）
 *
 * 【作用】
 *   在 PA4（DAC_OUT1）上输出指定形状、指定频率的模拟波形。
 *   形状：OFF / 正弦 / 三角 / 方波；频率：由面板决定（50Hz ~ 50kHz）。
 *
 * 【原理（一句话）】
 *   把"一个周期"预先算成 N 个电压码值存进表里，再用"定时器定时触发 + DMA 循环搬运"
 *   把表一个点一个点喂给 DAC，DAC 就把码值翻译成电压推出去。
 *
 * 【数据流】
 *   usWaveTable[N] ──DMA2_Ch3(循环模式)──> DAC->DHR12R1 ──TIM5 每 Ts 触发一次转换──> PA4 电压
 *        ▲                                                                             │
 *        └── 表里装一个周期；搬完 N 个点自动回表头 → 波形连绵不断                        │
 *                                                                                      ▼
 *   ┌───────────────── 三个量各自归谁管 ─────────────────┐
 *   │ N  = usWaveLength ：一个周期几个点 → 波形精细度 + DMA 搬运长度（多久回卷）    │
 *   │ fs = 1/Ts         ：TIM5 的 ARR 决定 → "画笔速度"，上限 500kHz               │
 *   │ f  = fs / N       ：最终输出频率（面板想调的就是它）                         │
 *   └──────────────────────────────────────────────────────┘
 *    频率低（≤500Hz）：fs = 1000×f，每周期满 1000 点 → 波形光滑
 *    频率高（>500Hz） ：fs 封顶 500kHz，只能减少 N（50kHz 时只剩 10 点）→ 出现台阶
 *
 * 【和谁有联系】
 *   上游(UI)    ：general_panel.c 的按键回调 → vWaveGenSetShape() / vWaveGenSetFrequency()
 *   上游(初始化)：main.c → vWaveGenInit()
 *   下游(硬件)  ：DAC(PA4) + TIM5(触发) + DMA2_Channel3(搬表)
 *   底层时钟/引脚：由 CubeMX 生成的 HAL_DAC_MspInit()（stm32f1xx_hal_msp.c）完成
 *
 * 【调用关系】
 *   vWaveGenInit() ──────────────┐
 *   vWaveGenSetShape() ──────────┼─> prvRestartDAC() ─┬─> prvAutoSampleRate()    (算 fs 和 N)
 *   vWaveGenSetFrequency() ──────┘                    ├─> prvPrepareWaveformData() (填表)
 *                                                     └─> HAL_DAC_Start_DMA() + TIM5 启动
 */

#include "waveform_generator.h"
#include "main.h"
#include <math.h>
#include <stdint.h>

#define PI 3.14159265358979f
#define WAVE_TABLE_MAX   1000        // 表容量：一个周期最多 1000 点
#define WAVE_FS_MAX      500000.0f   // 硬件上限：DAC+DMA 能稳定跑到的最高更新率 500kHz
#define WAVE_DAC_MID     1241        // 输出中点：1.0V 对应的码值 = 1.0/3.3*4095 ≈ 1241

DAC_HandleTypeDef hdac;          // DAC 句柄
DMA_HandleTypeDef hdma_dac_ch1;  // DAC 通道1 的 DMA 句柄（DMA2_Channel3）
TIM_HandleTypeDef htim5;         // 触发用定时器（TRGO = UPDATE）

static uint16_t usWaveTable[WAVE_TABLE_MAX];   // 一个周期的波形码值（0~4095 ↔ 0~3.3V）
static uint16_t usWaveLength = WAVE_TABLE_MAX; // 本周期实际用几个点 N（≤1000）
static uint8_t  ucWaveShape = 0;               // 形状：0-OFF 1-Sin 2-Triangle 3-Square
static float    fWaveFrequency = 1.0e3f;       // 目标频率，单位 Hz

static void prvAutoSampleRate(void);
static void prvPrepareWaveformData(void);
static void prvRestartDAC(void);

/*
 * @作用：初始化信号发生器（DAC + TIM5 + DMA2）
 * @调用：main() 里在 vTaskStartScheduler() 之前调用一次
 * @机制：三步搭好"硬件链路"，最后调 prvRestartDAC() 装载波形并开始输出
 *        ① DMA2_Channel3：循环模式，负责把表里的数一个个搬到 DAC
 *        ② DAC：触发源选 TIM5 的 TRGO（TIM5 每溢出一次，DAC 就转换一次）
 *        ③ TIM5：时钟源内部，TRGO = 更新事件（溢出即触发）
 * @注意：DAC 的时钟使能和 PA4 的模拟脚配置在 CubeMX 生成的 HAL_DAC_MspInit() 里，
 *        这里只负责"参数"和"DMA 链接"，所以不会和重新生成代码冲突
 */
void vWaveGenInit(void){
    DAC_ChannelConfTypeDef sConfig = {0};
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* 1. DAC_CH1 用 DMA2_Channel3 循环搬运波形表（DMA2 与 LCD/ADC/USART 用的 DMA1 不冲突） */
    __HAL_RCC_DMA2_CLK_ENABLE();
    hdma_dac_ch1.Instance = DMA2_Channel3;
    hdma_dac_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;      // 内存(表) → 外设(DAC)
    hdma_dac_ch1.Init.PeriphInc = DMA_PINC_DISABLE;          // 外设地址固定（一直是 DHR12R1）
    hdma_dac_ch1.Init.MemInc = DMA_MINC_ENABLE;              // 内存地址递增（表里一个个往后走）
    hdma_dac_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_dac_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD; // 表是 uint16_t，半字对齐
    hdma_dac_ch1.Init.Mode = DMA_CIRCULAR;                   // ★循环：搬完 N 个点自动回表头
    hdma_dac_ch1.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_dac_ch1) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_NVIC_SetPriority(DMA2_Channel3_IRQn, 5, 0); // 5 = FreeRTOS 允许调用 FromISR API 的最低优先级
    HAL_NVIC_EnableIRQ(DMA2_Channel3_IRQn);

    /* 2. DAC：由 TIM5 的 TRGO 触发，输出缓冲关闭（时钟与 PA4 由 CubeMX 生成的 HAL_DAC_MspInit 完成） */
    hdac.Instance = DAC;
    if (HAL_DAC_Init(&hdac) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_LINKDMA(&hdac, DMA_Handle1, hdma_dac_ch1); // HAL_DAC_Start_DMA 内部要用 DMA_Handle1，必须先链接
    sConfig.DAC_Trigger = DAC_TRIGGER_T5_TRGO;       // 触发源 = TIM5 TRGO（硬件定时，无需 CPU）
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_DISABLE; // 关闭内部输出缓冲（与参考工程一致）
    if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    /* 3. TIM5：PSC/ARR 决定采样率，TRGO = UPDATE（每溢出一次触发一次 DAC 转换） */
    htim5.Instance = TIM5;
    htim5.Init.Prescaler = 71;      // 先给个初值，真正数值由 prvAutoSampleRate() 按频率改写
    htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim5.Init.Period = 1;
    htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // ARR 改了立刻生效，不用等下一周期
    if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;    // 时钟来自 APB1（72MHz）
    if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;          // ★TRGO 输出"更新事件"给 DAC
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    prvRestartDAC(); // 装载波形并开始输出
}

/*
 * @作用：设置波形形状
 * @参数：ucShape - 0-OFF，1-Sin，2-Triangle，3-Square（和 general_panel 的枚举同编号）
 * @调用：general_panel.c 的 vadjust_general_panel()
 * @机制：只改状态，然后走统一的 prvRestartDAC()（停 → 重填表 → 重启）
 */
void vWaveGenSetShape(uint8_t ucShape){
    ucWaveShape = ucShape;
    prvRestartDAC();
}

/*
 * @作用：设置波形频率
 * @参数：fFrequency - 目标频率，单位 Hz
 * @调用：general_panel.c 的 vadjust_general_panel()
 * @机制：只改状态，然后走统一的 prvRestartDAC()（停 → 重算 fs 和 N → 重填表 → 重启）
 */
void vWaveGenSetFrequency(float fFrequency){
    fWaveFrequency = fFrequency;
    prvRestartDAC();
}

/*
 * @作用：根据目标频率反算"采样率 fs"和"每周期点数 N"，并写进 TIM5
 * @机制：
 *   ① 理想值：想让波形精细，就规定"一个周期 1000 点"
 *         → 需要 fs = 1000 × f  （例：1kHz 需要 1MHz，50kHz 需要 50MHz）
 *   ② 但 fs 有硬件上限 500kHz（DAC 建立时间 + DMA 带宽 + 总线）
 *         → 超了就"保频率、降点数"：fs = 500kHz，N = 500k / f
 *   ③ 把 fs 写进 TIM5：fs = 72MHz/(PSC+1)/(ARR+1)，这里 PSC=0 → ARR = 72M/fs − 1
 * @举例（f = 50kHz）：
 *     1000 × 50k = 50MHz > 500k → fs 取 500kHz，N = 500k/50k = 10
 *     ARR = 72M/500k − 1 = 143 → 每 2µs 推一个点 → 10 点 × 2µs = 20µs → 正好 50kHz
 * @注意：N 是取整的（+0.5 四舍五入），实际频率 = fs/N，两者不整除时会有小误差
 */
static void prvAutoSampleRate(void){
    uint32_t uSampleRate = 1000 * (uint32_t)fWaveFrequency; // 理想采样率 = 1000 点/周期

    usWaveLength = WAVE_TABLE_MAX; // 默认用满 1000 点/周期

    if(uSampleRate > (uint32_t)WAVE_FS_MAX)
    {
        // 采样率超上限：把采样率压在 500kHz，反过来缩短"每周期点数"来保住频率
        uSampleRate = (uint32_t)WAVE_FS_MAX;
        usWaveLength = (uint16_t)(WAVE_FS_MAX / fWaveFrequency + 0.5f);
    }

    // TIM5: 72MHz/(PSC+1)/(ARR+1) = fs  →  PSC=0, ARR = 72M/fs - 1
    __HAL_TIM_SET_PRESCALER(&htim5, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim5, (uint16_t)(72e6f / uSampleRate) - 1);
}

/*
 * @作用：按当前形状，填满一个周期的波形表 usWaveTable[0..usWaveLength-1]
 * @机制：
 *   表里存的是 DAC 码值（0~4095 ↔ 0~3.3V），所以每种波形都要先算出电压再换算：
 *        码值 = 电压 / 3.3 × 4095
 *   相位按 i/N × 360° 均匀铺满一个周期
 * @为什么电压都抬到 1V 附近：DAC 只能输出单极性 0~3.3V，没法输出负电压，
 *   所以用 1V 作"虚拟零点"，波形取 0~2V（正弦/三角是 1V ± 1V，方波是 1V/2V）
 * @四种形状：
 *   OFF      ：全部输出中点 1.0V（相当于输出零电平）
 *   正弦 Sin ：1.0 + sin(2πi/N)
 *   三角     ：前半周期 0→2V 线性上升，后半周期 2→0V 线性下降
 *   方波     ：前半周期 2V(2482)，后半周期 1V(1241)
 */
static void prvPrepareWaveformData(void){
    int i;

    if(ucWaveShape == 0) // OFF：输出中点电压
    {
        for(i=0; i<usWaveLength; i++)
        {
            usWaveTable[i] = WAVE_DAC_MID;
        }
    }
    else if(ucWaveShape == 1) // 正弦波：0 ~ 2V
    {
        for(i=0; i<usWaveLength; i++)
        {
            float fVolt = 1.0f + sinf((float)i / usWaveLength * 2.0f * PI);
            usWaveTable[i] = (uint16_t)(fVolt / 3.3f * 4095.0f + 0.5f); // +0.5 是四舍五入
        }
    }
    else if(ucWaveShape == 2) // 三角波：0 → 2V → 0
    {
        for(i=0; i<usWaveLength; i++)
        {
            float fVolt;
            if(i < usWaveLength/2)
            {
                fVolt = 4.0f * i / usWaveLength;            // 上升沿：0V → 2V
            }
            else
            {
                fVolt = 4.0f - 4.0f * i / usWaveLength;     // 下降沿：2V → 0V
            }
            usWaveTable[i] = (uint16_t)(fVolt / 3.3f * 4095.0f + 0.5f);
        }
    }
    else // 方波：1V / 2V
    {
        for(i=0; i<usWaveLength; i++)
        {
            usWaveTable[i] = (i < usWaveLength/2) ? 2482 : WAVE_DAC_MID;
        }
    }
}

/*
 * @作用：把"当前形状 + 当前频率"重新装载并启动输出（改参数后的统一出口）
 * @调用：vWaveGenInit()、vWaveGenSetShape()、vWaveGenSetFrequency()
 * @机制：停 → 算 → 填 → 启，四步固定顺序
 *   ① 停 TIM5 + 停 DAC DMA：必须先停，否则"表改到一半、DMA 还在搬"，输出的波形会错乱
 *   ② prvAutoSampleRate()：按频率算 fs(写 TIM5) 和 N
 *   ③ prvPrepareWaveformData()：按形状填表
 *   ④ HAL_DAC_Start_DMA(...)：启动 DMA 循环搬运
 *       参数含义：外设=DAC通道1，源=usWaveTable，长度=N（写进 DMA 的 CNDTR，决定多久回卷），
 *                 12 位右对齐（码值 0~4095 直接对应输出电压）
 *   ⑤ 启动 TIM5：真正开始"按节拍出波形"（TIM5 一启动，PA4 上立刻有输出）
 */
static void prvRestartDAC(void){
    HAL_TIM_Base_Stop(&htim5);
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);

    prvAutoSampleRate();     // 先按频率算采样率 fs 和每周期点数 N
    prvPrepareWaveformData();// 再按形状算波形表

    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (const uint32_t *)usWaveTable, usWaveLength, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(&htim5);
}

/*
 * @作用：DAC 的 DMA 传输完成中断
 * @机制：循环模式下，DMA 每搬完一个周期（N 个点）就产生一次"传输完成"进来一次；
 *        这里只把标志位清掉、调用 HAL 的回调，不做任何业务，所以耗时极短
 * @联系：不改变波形数据——波形的更新只发生在 prvRestartDAC()（用户改参数时）
 */
void DMA2_Channel3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_dac_ch1);
}
