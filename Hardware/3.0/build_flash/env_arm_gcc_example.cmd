@echo off
rem 与 3.0jijingoldplatform 一致：优先 D:\Tools\GNUArmEmbeddedToolchain\bin
rem 在要编译的窗口先执行本脚本，再运行 build.cmd

set "GCC_ARM=D:\Tools\GNUArmEmbeddedToolchain\bin"
if not exist "%GCC_ARM%\arm-none-eabi-gcc.exe" (
    set "GCC_ARM=%ProgramFiles(x86)%\GNU Arm Embedded Toolchain\10 2021.10\bin"
)
if not exist "%GCC_ARM%\arm-none-eabi-gcc.exe" (
    echo [错误] 未找到 arm-none-eabi-gcc，请编辑本 cmd 修改 GCC_ARM 路径。
    exit /b 1
)

set "PATH=%GCC_ARM%;%PATH%"
set "ARM_GCC_BIN=%GCC_ARM%"
echo [完成] 已加入 ARM GCC: %GCC_ARM%
where arm-none-eabi-gcc
echo.
echo 请在同一窗口执行:
echo   cd /d "%~dp0"
echo   build.cmd
exit /b 0
