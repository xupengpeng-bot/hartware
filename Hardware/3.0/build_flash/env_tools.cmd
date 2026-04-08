@echo off

rem 工具路径解析，顺序对齐 D:\Develop\hardware\3.0jijingoldplatform\tools\build_gcc.ps1

rem 由 build.cmd call，不设 setlocal，以便 ARM_GCC_BIN / CMAKE_EXE 留在当前会话中



rem ---------- CMake ----------

set "CMAKE_EXE="

where cmake >nul 2>&1 && for /f "delims=" %%i in ('where cmake') do set "CMAKE_EXE=%%i" & goto :cmake_ok

if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"

if not defined CMAKE_EXE if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles(x86)%\CMake\bin\cmake.exe"

if not defined CMAKE_EXE if exist "%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe" set "CMAKE_EXE=%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe"

if not defined CMAKE_EXE if exist "%ProgramData%\chocolatey\bin\cmake.exe" set "CMAKE_EXE=%ProgramData%\chocolatey\bin\cmake.exe"

if not defined CMAKE_EXE if exist "D:\Tools\CMake\bin\cmake.exe" set "CMAKE_EXE=D:\Tools\CMake\bin\cmake.exe"

if not defined CMAKE_EXE if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not defined CMAKE_EXE if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not defined CMAKE_EXE if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

:cmake_ok

if defined CMAKE_EXE for %%F in ("%CMAKE_EXE%") do set "PATH=%%~dpF;%PATH%"



rem ---------- GNU Arm Embedded（与旧工程 fallbackRoots 一致）----------

set "ARM_GCC_BIN="

where arm-none-eabi-gcc >nul 2>&1 && for /f "delims=" %%i in ('where arm-none-eabi-gcc') do call :set_arm_bin "%%i" & goto :arm_ok

if exist "D:\Tools\GNUArmEmbeddedToolchain\bin\arm-none-eabi-gcc.exe" set "ARM_GCC_BIN=D:\Tools\GNUArmEmbeddedToolchain\bin"

if not defined ARM_GCC_BIN if exist "%ProgramFiles(x86)%\GNU Arm Embedded Toolchain\10 2021.10\bin\arm-none-eabi-gcc.exe" set "ARM_GCC_BIN=%ProgramFiles(x86)%\GNU Arm Embedded Toolchain\10 2021.10\bin"

if not defined ARM_GCC_BIN if exist "D:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin\arm-none-eabi-gcc.exe" set "ARM_GCC_BIN=D:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin"

:arm_ok

rem 含 Program Files (x86) 的路径勿直接拼 %%ARM_GCC_BIN%%，用 for 引用避免解析错误

if defined ARM_GCC_BIN for %%A in ("%ARM_GCC_BIN%") do set "PATH=%%~A;%PATH%"



rem ---------- Ninja（CMake 常用；无 VS/nmake 时建议安装）----------

rem 本仓库 build_flash\tools\ninja.exe（随仓库或 ensure_ninja.ps1 自动下载）

if exist "%~dp0tools\ninja.exe" set "PATH=%~dp0tools;%PATH%"

if exist "D:\Tools\ninja\ninja.exe" set "PATH=D:\Tools\ninja;%PATH%"

if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\ninja.exe" set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"

if exist "%ProgramFiles%\Ninja\ninja.exe" set "PATH=%ProgramFiles%\Ninja;%PATH%"

if exist "%ProgramData%\chocolatey\bin\ninja.exe" set "PATH=%ProgramData%\chocolatey\bin;%PATH%"

if exist "%USERPROFILE%\scoop\shims\ninja.exe" set "PATH=%USERPROFILE%\scoop\shims;%PATH%"



rem ---------- st-flash（本机固定路径优先）----------

if exist "D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe" (

    set "PATH=D:\20251211\智能体\skills\stlink-1.8.0-win32\bin;%PATH%"

)

if exist "%~dp0tools\stlink-1.8.0-win32\bin\st-flash.exe" (

    for %%I in ("%~dp0tools\stlink-1.8.0-win32\bin") do set "PATH=%%~fI;%PATH%"

)

if exist "%~dp0..\..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe" (

    for %%I in ("%~dp0..\..\..\skills\stlink-1.8.0-win32\bin") do set "PATH=%%~fI;%PATH%"

)



rem ---------- STM32CubeProgrammer CLI（ST-Link 烧录，flash.cmd 依赖）----------

rem 可设置 STM32_CUBE_PROGRAMMER_BIN=...\bin 指向便携版；否则由 resolve_stm32_programmer.ps1 在「智能体」目录下搜索

set "STM32_CUBE_PROG_BIN="

if defined STM32_CUBE_PROGRAMMER_BIN (

    for %%A in ("%STM32_CUBE_PROGRAMMER_BIN%") do if exist "%%~A\STM32_Programmer_CLI.exe" set "STM32_CUBE_PROG_BIN=%%~A"

)

rem 仓库内便携版（ensure_stm32cubeprogrammer.ps1 或手动解压到此）
if not defined STM32_CUBE_PROG_BIN if exist "%~dp0tools\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (

    for %%A in ("%~dp0tools\STM32CubeProgrammer\bin") do set "STM32_CUBE_PROG_BIN=%%~A"

)

if not defined STM32_CUBE_PROG_BIN if exist "%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (

    set "STM32_CUBE_PROG_BIN=%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin"

)

if not defined STM32_CUBE_PROG_BIN if exist "%ProgramFiles(x86)%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (

    set "STM32_CUBE_PROG_BIN=%ProgramFiles(x86)%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin"

)

if not defined STM32_CUBE_PROG_BIN (

    for /f "usebackq delims=" %%p in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\resolve_stm32_programmer.ps1" -BuildFlashDir "%~dp0" 2^>nul`) do set "STM32_CUBE_PROG_BIN=%%p"

)

if defined STM32_CUBE_PROG_BIN for %%A in ("%STM32_CUBE_PROG_BIN%") do set "PATH=%%~A;%PATH%"

rem 供 flash.cmd 直接使用完整路径（不依赖 where）
set "STM32_PROG_CLI="
if defined STM32_CUBE_PROG_BIN for %%A in ("%STM32_CUBE_PROG_BIN%") do if exist "%%~A\STM32_Programmer_CLI.exe" set "STM32_PROG_CLI=%%~A\STM32_Programmer_CLI.exe"

rem 同步写入当前用户 PATH（新终端/IDE 可直接 where 到 STM32_Programmer_CLI）；设 STM32_SKIP_USER_PATH=1 可跳过
if defined STM32_CUBE_PROG_BIN if not defined STM32_SKIP_USER_PATH (

    for %%A in ("%STM32_CUBE_PROG_BIN%") do powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\ensure_stm32_programmer_user_path.ps1" -BinDir "%%~A" 2^>nul

)



exit /b 0



:set_arm_bin

for %%J in ("%~1") do set "ARM_GCC_BIN=%%~dpJ"

set "ARM_GCC_BIN=%ARM_GCC_BIN:~0,-1%"

exit /b

