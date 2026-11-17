# 环境体检（docs/env.md）

目标：换一台 Windows 机器，按本文逐条打勾，能在 5 分钟内从 clone 走到「串口看到版本行」。
实测环境：2026-11-17，Windows + PowerShell 5。

## 1. 工具链版本（本机实测）

| 工具 | 命令 | 本机结果 | 要求 |
|---|---|---|---|
| GCC | `arm-none-eabi-gcc --version` | 15.3.1（Arm GNU Toolchain 15.3.Rel1） | 支持 Cortex-M3 即可，建议 ≥10 |
| CMake | `cmake --version` | 4.4.0-rc3 | ≥3.22 |
| Ninja | `ninja --version` | 1.13.2（`C:\tools\ninja.exe`） | 任意较新版本 |
| OpenOCD | `openocd --version` | 0.12.0（sysprogs 构建） | 能识别 ST-Link 即可 |

> `openocd --version` 把版本写到 stderr，PowerShell 可能显示 `RemoteException`，只要能看到版本号就算通过。

## 2. 体检清单（6 项全绿才算环境就绪）

- [ ] `arm-none-eabi-gcc --version` 有输出
- [ ] `cmake --version` / `ninja --version` 有输出
- [ ] `openocd --version` 有输出
- [ ] `openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "adapter speed 1000; init; shutdown"` 能连上并识别芯片 ID
- [ ] `cmake --preset Debug && cmake --build --preset Debug` 产出 `build/Debug/FreertosGUILab.elf`
- [ ] 烧录后串口 115200 能看到 `fw ver ... | reset ...`

## 3. 构建（仓库根目录）

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

通过判据：

- 输出 `0 error 0 warning`；
- 末尾内存占用约 `FLASH 99968 B / 256 KB`、`RAM 44584 B / 48 KB`；
- 产物：`build/Debug/FreertosGUILab.elf`、`.map`。

`Debug` 预设使用 `cmake/gcc-arm-none-eabi.cmake`，HAL/CMSIS 来自仓库内 `third_party/stm32cube_f1`，**不需要**本机安装 STM32CubeF1。

## 4. 烧录与验证

```powershell
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg `
  -c "program build/Debug/FreertosGUILab.elf verify reset exit"
```

串口（USART1 PA9/PA10，115200 8N1）上电应输出：

```
fw ver 0.1.0 | build <编译日期> <编译时间> | reset POR
```

复位原因取值：`POR`（上电）、`PIN`（复位引脚）、`SOFT`（软复位/调试器）、`IWDG`/`WWDG`（看门狗）、`LPWR`。读取后已在固件里清标志，不会误报上一次复位。

无串口时的替代验证：连接调试器后在 GDB/调试控制台执行 `p hf_ctx`（HardFault 现场）或查看内存监控输出变量。

## 5. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `CMake was unable to find a build program corresponding to "Ninja"` | ninja 不在 PATH | 安装 ninja 或 `-DCMAKE_MAKE_PROGRAM=<ninja 绝对路径>` |
| `Could not find toolchain file: cmake/gcc-arm-none-eabi` | 手工传了相对路径 | 用绝对路径：`-DCMAKE_TOOLCHAIN_FILE=<repo>/cmake/gcc-arm-none-eabi.cmake`；或用 `cmake --preset Debug` |
| 配置时报 HAL 路径不存在 | 用 CubeMX 重新生成过，路径被写成绝对路径 | 把 `cmake/stm32cubemx/CMakeLists.txt` 里的路径改回 `${HAL_ROOT}/...` |
| 串口无输出 | 接线/波特率/USART1 DMA 中断未使能 | 确认 PA9→RX、PA10→TX、115200；确认 `App_USART1_Init()` 在调度器前调用 |
| 烧录后反复复位/花屏 | IWDG 被打开但无喂狗点 | 保持 `//MX_IWDG_Init()` 注释；启用前补 `HAL_IWDG_Refresh()` |

## 6. 冷构建验证（交付前做一次）

```powershell
$tmp = "$env:TEMP\FreertosGUILab_clone"
git clone <repo-url> $tmp
Set-Location $tmp
cmake --preset Debug
cmake --build --preset Debug
```

预期：全新目录、无 `build/`、无本机 CubeMX 依赖，仍得到 `0 error 0 warning`。
