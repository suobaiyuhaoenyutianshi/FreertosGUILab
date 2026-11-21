# 1. 收集所有 .c（只收集 .c，不要收集 .h）
# CONFIGURE_DEPENDS：构建前自动重新扫描目录，新增/删除 .c 无需手动重跑 cmake --preset Debug
file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/freeRTOS/Src/*.c" 
     "${CMAKE_CURRENT_SOURCE_DIR}/key/*.c" 
    "${CMAKE_CURRENT_SOURCE_DIR}/LCD/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/DmmAndGui/*.c"
      # 如果有就加
)
# 1.1 Core/Src 只收本项目自己写的文件。
# cmake/stm32cubemx/CMakeLists.txt 已经负责这些文件：main.c / it.c / msp.c /
# timebase / sysmem.c / syscalls.c 加入主目标，system_stm32f1xx.c 加入
# STM32_Drivers 目标。这里若重复收集，system_stm32f1xx.c 会在两个目标各编译
# 一份 -> 链接时报 multiple definition。
file(GLOB USER_SOURCES_CORE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.c")
list(FILTER USER_SOURCES_CORE EXCLUDE REGEX
    "/main\\.c$|/stm32f1xx_it\\.c$|/stm32f1xx_hal_msp\\.c$|/stm32f1xx_hal_timebase_tim\\.c$|/system_stm32f1xx\\.c$|/sysmem\\.c$|/syscalls\\.c$")
list(APPEND USER_SOURCES ${USER_SOURCES_CORE})
# 2. 编译源文件
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${USER_SOURCES})

# 裸机实验开关：只影响 main/dmm/key/bmcore 的宿主路径
if(BM_BAREMETAL)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE BM_BAREMETAL=1)
    message(STATUS "Bare-metal lab build: BM_BAREMETAL=1")
endif()

# 3. 添加头文件搜索路径（让编译器能找到你写的 .h）"${CMAKE_CURRENT_SOURCE_DIR}/"      # 对应上面的 .c 文件夹
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/freeRTOS/Inc"
     "${CMAKE_CURRENT_SOURCE_DIR}/key"
      "${CMAKE_CURRENT_SOURCE_DIR}/LCD"
      "${CMAKE_CURRENT_SOURCE_DIR}/DRivers" 
      "${CMAKE_CURRENT_SOURCE_DIR}/DmmAndGui"
)
# =========================================================
# 2. 静态库搜索路径 + 链接 lcd.a
# =========================================================
# 告诉链接器去哪里找 lcd.a
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/DRivers"
)

# 4. 链接外部 .a 静态库（如果需要 DSP 数学库之类的） #"${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Lib/GCC/libarm_cortexM3l_math.a"
target_link_libraries(${CMAKE_PROJECT_NAME}
    "${CMAKE_CURRENT_SOURCE_DIR}/DRivers/lcd.a"
   
)

# 5. 保留你的浮点打印支持（必须保留）
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)