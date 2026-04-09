@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

rem 只烧 controller_fw_standalone.bin 到 0x08000000（整片 APP，无需 bootloader）。排障时优先试此脚本。

if defined BUILD_FLASH_OUT (set "BUILD_DIR=!BUILD_FLASH_OUT!") else (set "BUILD_DIR=%LOCALAPPDATA%\hw_embedded_build\out\build")

set "BIN=%BUILD_DIR%\controller_fw_standalone.bin"

if not exist "!BIN!" (
    echo [错误] 未找到 !BIN!
    echo 请先 build.cmd ^(会编 controller_fw_standalone^) 或: cmake --build 构建目录 --target controller_fw_standalone
    exit /b 1
)

call "%~dp0flash.cmd" "!BIN!"
exit /b %ERRORLEVEL%
