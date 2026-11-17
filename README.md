# FreertosGUILab

STM32F103RCT6 + FreeRTOS 的示波器/万用表 GUI 固件：SPI+DMA 刷屏、ADC 双缓冲采样、DAC 波形发生器、按键队列与串口 DMA 日志。

这是一个**可复现的学习工程**：clone 下来能编、能烧、能按本文档看到版本行。仓库自带上游 HAL/CMSIS 源码，不需要安装 STM32CubeMX 或 STM32CubeF1 包。

## 1. 硬件与接线

| 项 | 说明 |
|---|---|
| MCU | STM32F103RCT6，LQFP64，256 KB Flash / 48 KB RAM，HSE 8 MHz → 72 MHz |
| 调试器 | ST-Link（SWD：PA13/PA14） |
| 串口 | USART1，PA9(TX)/PA10(RX)，115200 8N1，USB-TTL |
| LCD | SPI1 + DMA，240×320，背光/复位/RS 见 `FreertosGUILab.ioc` |
| 模拟输入 | ADC1 注入组（CH8/CH9 + VREFINT），TIM2 TRGO 触发 |
| 波形输出 | DAC_CH1(PA4)，TIM5 TRGO 触发，DMA2_Channel3 循环搬运 |
| 按键 | KEY1~KEY4（公共队列）、KEYU/KEYD（慢回调专用队列） |

## 2. 5 分钟跑通

前置：`arm-none-eabi-gcc`、`cmake`(≥3.22)、`ninja`、`openocd` 都在 PATH 里。逐项体检见 [docs/env.md](docs/env.md)。

```powershell
# 1. 配置 + 构建（在仓库根目录）
cmake --preset Debug
cmake --build --preset Debug

# 2. 烧录（ST-Link + OpenOCD）
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg `
  -c "program build/Debug/FreertosGUILab.elf verify reset exit"

# 3. 串口 115200 8N1，上电第一行应看到：
#    fw ver 0.1.0 | build <编译日期> <编译时间> | reset POR
```

构建成功的判据：`0 error 0 warning`，链接末尾打印内存占用（当前约 FLASH 99.9 KB / 256 KB，RAM 44.6 KB / 48 KB）。

没有 USB-TTL 时，可用调试器读 RAM 中的日志/故障现场：`p hf_ctx`（HardFault 现场结构体，见 `Core/Src/stm32f1xx_it.c`）。

## 3. 目录

```
Core/            启动、CubeMX 外设初始化、串口 DMA、内存监控、HardFault 现场
DmmAndGui/       万用表面板、波形捕获（双缓冲 + 浮点换算）、DAC 信号发生器
LCD/             ILI9341 类屏的 SPI+DMA 驱动任务
key/             按键扫描、公共队列消费者、慢回调队列消费者
freeRTOS/        FreeRTOS 内核（heap_4）
third_party/     STM32CubeF1 1.8.7 的 HAL 与 CMSIS 源码（随仓库提供）
DRivers/         lcd.a 预编译库与头文件
cmake/           工具链与 CubeMX 生成的构建脚本
docs/            环境体检、调试记录、面试复盘等文档
```

## 4. 构建配置

- `Debug`：`-O0 -g3 -Wall -Wextra -Wpedantic`；`Release`：`-Os -g0`。
- 链接脚本：`stm32f103rctx_flash.ld`（Flash 256 KB @0x08000000，RAM 48 KB @0x20000000）。
- HAL/CMSIS 路径由 `cmake/stm32cubemx/CMakeLists.txt` 里的 `HAL_ROOT` 指向 `third_party/stm32cube_f1`。
- 若用 STM32CubeMX 重新生成代码，它会把 HAL 路径写成你本机的绝对路径；生成后需要把 `MX_Include_Dirs` / `STM32_Drivers_Src` 改回 `${HAL_ROOT}/...`。

## 5. 当前状态与已知限制

- 已验证：干净目录 clone → 配置 → 构建 `0 error 0 warning`；仓库 `git status` 干净。
- 启动版本行：`fw ver / build date / reset cause`（上电、看门狗、软复位可区分）。
- RAM 占用 90.7%（44,584 B / 48 KB），新增任务或大缓冲前先看内存余量。
- IWDG 已在 CubeMX 中配置但未启用（全工程没有喂狗点），启用前必须补 `HAL_IWDG_Refresh()`。
- `main.c` 里的 `EXERCISE_CRASH_ENABLE` / `EXERCISE_JUMP_ENABLE` 是故障定位练习开关，默认关闭。
- 尚未包含：host 单元测试、CI、示波器实测定位记录（对应三个月主线 P3；本仓库先把「clone 即构建 + 版本行 + 干净提交」做完）。
