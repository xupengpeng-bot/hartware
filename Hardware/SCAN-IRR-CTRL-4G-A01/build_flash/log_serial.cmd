@echo off
chcp 65001 >nul
setlocal

set "SCRIPT_DIR=%~dp0"

if "%~1"=="" (
    if defined SERIAL_LOG_PORT (
        set "SERIAL_PORT=%SERIAL_LOG_PORT%"
    ) else (
        echo [error] missing COM port. Usage: log_serial.cmd COM7 [baud]
        echo [hint] current COM ports:
        powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\serial_capture.ps1" -ListPorts
        exit /b 1
    )
) else (
    set "SERIAL_PORT=%~1"
)

if "%~2"=="" (
    if defined SERIAL_LOG_BAUD (
        set "SERIAL_BAUD=%SERIAL_LOG_BAUD%"
    ) else (
        set "SERIAL_BAUD=9600"
    )
) else (
    set "SERIAL_BAUD=%~2"
)

echo [info] serial capture port: %SERIAL_PORT%
echo [info] serial capture baud: %SERIAL_BAUD%
if defined SERIAL_LOG_DIR echo [info] serial log dir: %SERIAL_LOG_DIR%

if defined SERIAL_LOG_DIR (
    if defined SERIAL_LOG_SESSION_NAME (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\serial_capture.ps1" -Port "%SERIAL_PORT%" -Baud %SERIAL_BAUD% -OutputDir "%SERIAL_LOG_DIR%" -SessionName "%SERIAL_LOG_SESSION_NAME%"
    ) else (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\serial_capture.ps1" -Port "%SERIAL_PORT%" -Baud %SERIAL_BAUD% -OutputDir "%SERIAL_LOG_DIR%"
    )
) else (
    if defined SERIAL_LOG_SESSION_NAME (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\serial_capture.ps1" -Port "%SERIAL_PORT%" -Baud %SERIAL_BAUD% -SessionName "%SERIAL_LOG_SESSION_NAME%"
    ) else (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\serial_capture.ps1" -Port "%SERIAL_PORT%" -Baud %SERIAL_BAUD%
    )
)

exit /b %ERRORLEVEL%
