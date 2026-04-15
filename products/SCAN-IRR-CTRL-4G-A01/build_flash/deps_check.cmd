@echo off

chcp 65001 >nul

setlocal EnableDelayedExpansion

echo === 固件构建 / 烧录依赖检查 ===

echo.



call "%~dp0env_tools.cmd"



if defined CMAKE_EXE (echo [OK] CMake: !CMAKE_EXE!) else (echo [缺] CMake — 请安装 https://cmake.org/download/ 或 winget install Kitware.CMake)

if defined ARM_GCC_BIN (echo [OK] arm-none-eabi-gcc 目录: !ARM_GCC_BIN!) else (echo [缺] GNU Arm Embedded Toolchain — 见 env_arm_gcc_example.cmd)



set "STM32_OK=0"

if defined STM32_PROG_CLI if exist "!STM32_PROG_CLI!" (

  echo [OK] STM32_Programmer_CLI: !STM32_PROG_CLI!

  set "STM32_OK=1"

)

if "!STM32_OK!"=="0" (

  where STM32_Programmer_CLI >nul 2>&1 && (

    echo [OK] STM32CubeProgrammer CLI 在 PATH 中

    set "STM32_OK=1"

  )

)

if "!STM32_OK!"=="0" if exist "%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (

  echo [OK] STM32_Programmer_CLI: 已安装 ^(未加 PATH 时 flash.cmd 会尝试补全^)

  set "STM32_OK=1"

)

if "!STM32_OK!"=="0" echo [可选] ST-Link 烧录需 STM32CubeProgrammer: https://www.st.com/en/development-tools/stm32cubeprog.html



where openocd >nul 2>&1 && echo [OK] OpenOCD 在 PATH 中 || echo [可选] FLASH_TOOL=OPENOCD 时需安装 OpenOCD



if exist "D:\Tools\stm32flash\stm32flash.exe" (echo [OK] stm32flash: D:\Tools\stm32flash\) else (

  where stm32flash >nul 2>&1 && echo [OK] stm32flash 在 PATH 中 || echo [可选] FLASH_TOOL=SERIAL 时需 stm32flash ^(见 tools\ensure_stm32flash.ps1^)

)



where ninja >nul 2>&1

if not errorlevel 1 (

  echo [OK] ninja 在 PATH 中（CMake 推荐生成器）

) else (

  where nmake >nul 2>&1

  if not errorlevel 1 (

    echo [OK] nmake 可用（CMake 可用 NMake）

  ) else (

    echo [建议] 安装 Ninja: winget install Ninja-build.Ninja  或解压到 D:\Tools\ninja\

  )

)



echo.

echo 说明: 无 Visual Studio 时 CMake 需 Ninja 或 nmake；SERIAL 烧录需 COM 与 FLASH_BAUD。

exit /b 0

