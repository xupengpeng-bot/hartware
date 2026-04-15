@echo off
chcp 65001 >nul
setlocal

if "%~1"=="" (
    echo [错误] 用法: build_and_ota.cmd ^<IMEI^>
    echo [提示] 示例: build_and_ota.cmd 861295087573980
    exit /b 1
)

set "OTA_IMEI=%~1"

call "%~dp0build.cmd"
if errorlevel 1 exit /b 1

if defined BUILD_FLASH_OUT (
    set "BUILD_DIR=%BUILD_FLASH_OUT%"
) else (
    set "BUILD_DIR=%LOCALAPPDATA%\hw_embedded_build\out\build"
)

set "BIN=%BUILD_DIR%\controller_fw.bin"
if not exist "%BIN%" (
    echo [错误] 未找到构建产物: %BIN%
    exit /b 1
)

set "BACKEND_DIR=%~dp0..\..\houjinongfuai-working\backend"
if not exist "%BACKEND_DIR%\package.json" (
    echo [错误] 未找到 backend 目录: %BACKEND_DIR%
    exit /b 1
)

pushd "%BACKEND_DIR%"
call npx ts-node .\scripts\ota-upgrade-device.ts --imei "%OTA_IMEI%" --bin "%BIN%"
set "RC=%ERRORLEVEL%"
popd

exit /b %RC%
