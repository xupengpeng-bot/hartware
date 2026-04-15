@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

rem 与 build.cmd 相同环境，但额外生成 bootloader（改 code\bootloader 或首次出盘时执行）

set "SCRIPT_DIR=%~dp0"
set "CODE_DIR=%SCRIPT_DIR%..\code"
if defined BUILD_FLASH_OUT (set "BUILD_DIR=!BUILD_FLASH_OUT!") else (set "BUILD_DIR=%LOCALAPPDATA%\hw_embedded_build\out\build")

if not exist "%CODE_DIR%\CMakeLists.txt" (
    echo [错误] 未找到 CMakeLists.txt: "%CODE_DIR%\CMakeLists.txt"
    exit /b 1
)

if not exist "%SCRIPT_DIR%tools\ninja.exe" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\ensure_ninja.ps1"
    if errorlevel 1 exit /b 1
)

call "%SCRIPT_DIR%env_tools.cmd"

if not defined CMAKE_GENERATOR if exist "%SCRIPT_DIR%tools\ninja.exe" set "CMAKE_GENERATOR=Ninja"
if not defined CMAKE_GENERATOR where ninja >nul 2>&1 && set "CMAKE_GENERATOR=Ninja"

if not defined CMAKE_EXE (
    echo [错误] 未找到 cmake.exe
    exit /b 1
)

if not defined CMAKE_TOOLCHAIN_FILE if defined ARM_GCC_BIN (
    set "CMAKE_TOOLCHAIN_FILE=%SCRIPT_DIR%toolchain-arm-none-eabi.cmake"
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

if defined CMAKE_TOOLCHAIN_FILE (
    if defined CMAKE_GENERATOR (
        cmake -S "%CODE_DIR%" -B "%BUILD_DIR%" -G "!CMAKE_GENERATOR!" -DCMAKE_TOOLCHAIN_FILE="%CMAKE_TOOLCHAIN_FILE%" -DCMAKE_BUILD_TYPE=Release
    ) else (
        cmake -S "%CODE_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%CMAKE_TOOLCHAIN_FILE%" -DCMAKE_BUILD_TYPE=Release
    )
) else (
    echo [错误] build_all 需要 ARM 交叉编译，请设置 ARM_GCC_BIN 或 toolchain。
    exit /b 1
)
if errorlevel 1 exit /b 1

echo [信息] 构建 bootloader + controller_fw ^(全量^)...

cmake --build "%BUILD_DIR%" --config Release --target bootloader
if errorlevel 1 (
    echo [错误] bootloader 编译失败。
    exit /b 1
)
cmake --build "%BUILD_DIR%" --config Release --target controller_fw
if errorlevel 1 (
    echo [错误] controller_fw 编译失败。
    exit /b 1
)

echo.
echo [完成] 产物目录: %BUILD_DIR%
echo        - bootloader.bin / bootloader.hex
echo        - controller_fw.bin
exit /b 0
