@echo off

chcp 65001 >nul

setlocal EnableDelayedExpansion



set "SCRIPT_DIR=%~dp0"

rem 与 build.cmd 默认产物路径一致

if defined BUILD_FLASH_OUT (

    set "BUILD_DIR=!BUILD_FLASH_OUT!"

) else (

    set "BUILD_DIR=%LOCALAPPDATA%\hw_embedded_build\out\build"

)



call "%SCRIPT_DIR%env_tools.cmd"

rem ST-Link 烧录默认走 st-flash；STM32CubeProgrammer 仅当 USE_STM32_CUBE_CLI=1



rem 参数: [固件.bin路径] [串口 COMx，仅 SERIAL 模式需要]

if "%~1"=="" (

    set "BIN_FILE=%BUILD_DIR%\controller_fw.bin"

) else (

    set "BIN_FILE=%~1"

)



rem 整片从 0 开始烧录（如仅 bootloader.bin）用 0x08000000；仅烧录 APP（controller_fw.bin）须与分区一致：

rem   set FLASH_ADDR=0x08010000

rem 仅烧 APP(controller_fw) 且已有 bootloader 时请用: set FLASH_ADDR=0x08010000

if not defined FLASH_ADDR (
    for %%F in ("!BIN_FILE!") do set "BIN_NAME=%%~nxF"
    if /I "!BIN_NAME!"=="controller_fw.bin" (
        set "FLASH_ADDR=0x08010000"
    ) else (
        set "FLASH_ADDR=0x08000000"
    )
)



if not exist "!BIN_FILE!" (

    echo [error] missing bin: "!BIN_FILE!"

    echo 请先运行 build.cmd 成功生成 controller_fw.bin，或将 .bin 完整路径作为参数传入:

    echo   flash.cmd "D:\path\to\firmware.bin"

    exit /b 1

)



echo [info] bin: !BIN_FILE!

echo [info] addr: !FLASH_ADDR!



rem ---------------------------------------------------------------------------

rem FLASH_TOOL:

rem   STLINK  - 默认仅用 st-flash（开源 ST-Link）。若需 STM32CubeProgrammer CLI，请 set USE_STM32_CUBE_CLI=1

rem   OPENOCD - OpenOCD

rem   SERIAL  - stm32flash 串口烧录（与 3.0jijingoldplatform\BUILD.bat 一致，优先 D:\Tools\stm32flash）

rem ---------------------------------------------------------------------------

if not defined FLASH_TOOL set "FLASH_TOOL=STLINK"



if /I "!FLASH_TOOL!"=="STLINK" goto :DO_STLINK

if /I "!FLASH_TOOL!"=="OPENOCD" goto :DO_OPENOCD

if /I "!FLASH_TOOL!"=="SERIAL" goto :DO_SERIAL



echo [错误] 未知 FLASH_TOOL=!FLASH_TOOL!，请设为 STLINK、OPENOCD 或 SERIAL

exit /b 1



:DO_STLINK

rem 默认仅 st-flash；USE_STM32_CUBE_CLI=1 时改用 STM32CubeProgrammer CLI
if /I "!USE_STM32_CUBE_CLI!"=="1" goto :DO_STLINK_STM32_CLI



:DO_STLINK_STFLASH

set "STFLASH_EXE="

rem 可选覆盖：set STFLASH_EXE_USER=完整路径\st-flash.exe
if defined STFLASH_EXE_USER if exist "!STFLASH_EXE_USER!" set "STFLASH_EXE=!STFLASH_EXE_USER!"

rem 本机固定路径（你提供的 skills\stlink 目录，不再调用 PowerShell 搜索）
if not defined STFLASH_EXE if exist "D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"

if not defined STFLASH_EXE if exist "!SCRIPT_DIR!tools\st-flash.exe" for %%F in ("!SCRIPT_DIR!tools\st-flash.exe") do set "STFLASH_EXE=%%~fF"

if not defined STFLASH_EXE if exist "!SCRIPT_DIR!tools\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("!SCRIPT_DIR!tools\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"

if not defined STFLASH_EXE if exist "!SCRIPT_DIR!..\..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("!SCRIPT_DIR!..\..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"

if not defined STFLASH_EXE if exist "!SCRIPT_DIR!..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("!SCRIPT_DIR!..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"

if not defined STFLASH_EXE where st-flash.exe >nul 2>&1 && for /f "delims=" %%i in ('where st-flash.exe') do set "STFLASH_EXE=%%i"

if not defined STFLASH_EXE where st-flash >nul 2>&1 && for /f "delims=" %%i in ('where st-flash') do set "STFLASH_EXE=%%i"

if not defined STFLASH_EXE (

    echo [错误] 未找到 st-flash.exe

    echo.

    echo 请确认存在: D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe

    echo 或 set STFLASH_EXE_USER=你的 st-flash.exe 完整路径

    echo.

    echo 若必须用 STM32CubeProgrammer CLI:  set USE_STM32_CUBE_CLI=1

    echo.

    exit /b 1

)

rem 默认含 --flash=256k 对应 STM32F103RCT6；F103C8(64k) 请 set STFLASH_FLAGS=--connect-under-reset --freq=100 --flash=64k
rem 板子 NRST 未接时可 set STFLASH_SKIP_RESET=1 或 STFLASH_FLAGS 加 --hot-plug
if not defined STFLASH_FLAGS set "STFLASH_FLAGS=--connect-under-reset --freq=100 --flash=256k"

echo [exec] st-flash write

"!STFLASH_EXE!" !STFLASH_FLAGS! write "!BIN_FILE!" !FLASH_ADDR!
if errorlevel 1 goto :STFLASH_FAIL

if not defined STFLASH_SKIP_RESET (

    "!STFLASH_EXE!" !STFLASH_FLAGS! reset 2>nul

)

goto :STFLASH_OK

:STFLASH_FAIL

echo [error] st-flash failed. Check NRST wiring, power, SWD. Try STFLASH_FLAGS with --hot-plug

exit /b 1

:STFLASH_OK

goto :DONE



:DO_STLINK_STM32_CLI

set "STM32_EXE="

if defined STM32_PROG_CLI if exist "!STM32_PROG_CLI!" set "STM32_EXE=!STM32_PROG_CLI!"

if not defined STM32_EXE where STM32_Programmer_CLI.exe >nul 2>&1 && for /f "delims=" %%i in ('where STM32_Programmer_CLI.exe') do set "STM32_EXE=%%i" & goto :stm32_got

if not defined STM32_EXE where STM32_Programmer_CLI >nul 2>&1 && for /f "delims=" %%i in ('where STM32_Programmer_CLI') do set "STM32_EXE=%%i" & goto :stm32_got

:stm32_got

if not defined STM32_EXE (

    echo [错误] USE_STM32_CUBE_CLI=1 但未找到 STM32_Programmer_CLI.exe

    exit /b 1

)

echo [执行] STM32 SWD 烧录 ^(STM32CubeProgrammer CLI^):

echo        "!STM32_EXE!"

"!STM32_EXE!" -c port=SWD -w "%BIN_FILE%" %FLASH_ADDR% -v -rst

if errorlevel 1 (

    echo [错误] 烧录失败。检查接线、驱动、BOOT 与芯片型号。

    exit /b 1

)

goto :DONE



:DO_OPENOCD

where openocd >nul 2>&1

if errorlevel 1 (

    echo [错误] 未找到 openocd，请先安装 OpenOCD 并加入 PATH。

    exit /b 1

)

if not defined OPENOCD_INTERFACE set "OPENOCD_INTERFACE=interface/stlink.cfg"

if not defined OPENOCD_TARGET set "OPENOCD_TARGET=target/stm32f1x.cfg"

echo [执行] openocd 烧录...

openocd -f "!OPENOCD_INTERFACE!" -f "!OPENOCD_TARGET!" -c "program \"%BIN_FILE%\" %FLASH_ADDR% verify reset exit"

if errorlevel 1 (

    echo [错误] OpenOCD 烧录失败。请按芯片修改 OPENOCD_INTERFACE / OPENOCD_TARGET。

    exit /b 1

)

goto :DONE



:DO_SERIAL

set "STM32FLASH="

if exist "D:\Tools\stm32flash\stm32flash.exe" set "STM32FLASH=D:\Tools\stm32flash\stm32flash.exe"

if not defined STM32FLASH (

    for /f "usebackq delims=" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\ensure_stm32flash.ps1" -ResolveOnly 2^>nul`) do set "STM32FLASH=%%i"

)

if not defined STM32FLASH (

    where stm32flash >nul 2>&1 && for /f "delims=" %%i in ('where stm32flash') do set "STM32FLASH=%%i"

)

if not defined STM32FLASH (

    echo [错误] 未找到 stm32flash.exe。请将 stm32flash 放到 D:\Tools\stm32flash\ 或加入 PATH。

    echo 可参考 3.0jijingoldplatform\tools\ensure_stm32flash.ps1

    exit /b 1

)



if not "%~2"=="" (

    set "COMPORT=%~2"

    goto :com_ok

)

if defined COMPORT goto :com_ok

set /p COMPORT=Enter COM port (e.g. COM5): 

:com_ok

if "!COMPORT!"=="" (

    echo [错误] 需要串口号。用法: flash.cmd 固件.bin COM5  或  set COMPORT=COM5

    exit /b 1

)



if not defined FLASH_BAUD set "FLASH_BAUD=57600"

echo [执行] 串口烧录: "!STM32FLASH!" 端口=!COMPORT! 波特率=!FLASH_BAUD!

echo [提示] 接线/BOOT 与旧工程一致时参见 3.0jijingoldplatform\BUILD.bat 说明。

"!STM32FLASH!" -b !FLASH_BAUD! -w "%BIN_FILE%" -S %FLASH_ADDR% -v !COMPORT! -g %FLASH_ADDR% -R

if errorlevel 1 (

    echo [错误] 串口烧录失败。可尝试: set FLASH_BAUD=115200

    exit /b 1

)

goto :DONE



:DONE

echo [完成] 烧录结束。

if defined SERIAL_LOG_PORT (
    echo [info] starting serial capture on !SERIAL_LOG_PORT! ...
    if not defined SERIAL_LOG_BAUD set "SERIAL_LOG_BAUD=115200"
    call "%SCRIPT_DIR%log_serial.cmd" "!SERIAL_LOG_PORT!" "!SERIAL_LOG_BAUD!"
    exit /b %ERRORLEVEL%
)

exit /b 0

