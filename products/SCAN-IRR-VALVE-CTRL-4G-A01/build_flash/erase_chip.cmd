@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%env_tools.cmd"

rem 全片擦除（Mass erase）— 与 flash.cmd 使用相同的 st-flash 路径与 STFLASH_FLAGS
rem F103RCT6 默认 --flash=256k；F103C8 请: set STFLASH_FLAGS=--connect-under-reset --freq=100 --flash=64k

set "STFLASH_EXE="
if defined STFLASH_EXE_USER if exist "!STFLASH_EXE_USER!" set "STFLASH_EXE=!STFLASH_EXE_USER!"
if not defined STFLASH_EXE if exist "D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"
if not defined STFLASH_EXE if exist "!SCRIPT_DIR!tools\st-flash.exe" for %%F in ("!SCRIPT_DIR!tools\st-flash.exe") do set "STFLASH_EXE=%%~fF"
if not defined STFLASH_EXE if exist "!SCRIPT_DIR!tools\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("!SCRIPT_DIR!tools\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"
if not defined STFLASH_EXE if exist "!SCRIPT_DIR!..\..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("!SCRIPT_DIR!..\..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"
if not defined STFLASH_EXE if exist "!SCRIPT_DIR!..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe" for %%F in ("!SCRIPT_DIR!..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe") do set "STFLASH_EXE=%%~fF"
if not defined STFLASH_EXE where st-flash.exe >nul 2>&1 && for /f "delims=" %%i in ('where st-flash.exe') do set "STFLASH_EXE=%%i"
if not defined STFLASH_EXE where st-flash >nul 2>&1 && for /f "delims=" %%i in ('where st-flash') do set "STFLASH_EXE=%%i"

if not defined STFLASH_EXE (
    echo [error] st-flash.exe not found
    exit /b 1
)

if not defined STFLASH_FLAGS set "STFLASH_FLAGS=--connect-under-reset --freq=100 --flash=256k"

echo [info] Full chip erase via st-flash
echo [info] exe: !STFLASH_EXE!
echo [info] flags: !STFLASH_FLAGS!
echo 提示: 若连不上，可按住板子复位键再执行本脚本，或接好 ST-Link 的 NRST。
echo.

"!STFLASH_EXE!" !STFLASH_FLAGS! erase
if errorlevel 1 (
    echo [error] erase failed. Try: set STFLASH_FLAGS=--connect-under-reset --freq=100 --flash=256k --hot-plug
    exit /b 1
)

echo [ok] erase done. Flash is empty; run flash.cmd to program again.
exit /b 0
