@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

rem 先烧 bootloader@0x08000000，再烧 APP@0x08010000。仅烧 controller_fw.bin 到 0x08010000 而没有本 bootloader 时，MCU 上电不会进入 APP。

set "SCRIPT_DIR=%~dp0"
if defined BUILD_FLASH_OUT (set "BUILD_DIR=!BUILD_FLASH_OUT!") else (set "BUILD_DIR=%LOCALAPPDATA%\hw_embedded_build\out\build")

set "BL=!BUILD_DIR!\bootloader.bin"
set "APP=!BUILD_DIR!\controller_fw.bin"

if not exist "!BL!" (
    echo [错误] 未找到 !BL!
    echo 请先运行 build_all.cmd 或: cmake --build 构建目录 --target bootloader
    exit /b 1
)
if not exist "!APP!" (
    echo [错误] 未找到 !APP!
    echo 请先运行 build.cmd 生成 controller_fw.bin
    exit /b 1
)

rem flash.cmd 按文件名自动选地址：bootloader.bin -^> 0x08000000，controller_fw.bin -^> 0x08010000
call "%SCRIPT_DIR%flash.cmd" "!BL!"
if errorlevel 1 exit /b 1

call "%SCRIPT_DIR%flash.cmd" "!APP!"
if errorlevel 1 exit /b 1

echo.
echo [完成] 已烧录 bootloader ^(0x08000000^) + controller_fw ^(0x08010000^)
exit /b 0
