cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME FreertosGUILab)


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages


# Core project settings
project(${CMAKE_PROJECT_NAME})
enable_language(C ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# 裸机最小闭环实验：只切换宿主，不改变默认 Debug/Release 构建
option(BM_BAREMETAL "Build the bare-metal lab (domain A + UART)" OFF)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
include(${CMAKE_CURRENT_SOURCE_DIR}/user_sources.cmake)

# ---------------------------------------------------------------------------
# 把 compile_commands.json 复制到工程根目录，供 clangd 自动发现。
# 原因：.clangd 里的 CompileFlags.CompilationDatabase 在部分 clangd（如本机的
# 21.1.8）上只对头文件推断生效、对 .c 文件查不到，导致整个工程按"无编译参数"
# 解析、报一堆假错误。clangd 会从源文件目录逐级向上找 compile_commands.json，
# 放在根目录最稳。每次链接成功后刷新，保证和 build 目录里的那份一致。
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
)

