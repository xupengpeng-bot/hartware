@echo off
chcp 65001 >nul
cd /d "%~dp0"

rem 优先使用 python，其次 py -3（Windows 商店 / 安装器）
where python >nul 2>&1 && goto :run_py
where py >nul 2>&1 && goto :run_py_launcher

echo [错误] 未找到 Python。请安装 Python 3.8+ 并勾选 tcl/tk，或从 Microsoft Store 安装。
pause
exit /b 1

:run_py
python "%~dp0tools\flash_stlink_gui.py"
exit /b %ERRORLEVEL%

:run_py_launcher
py -3 "%~dp0tools\flash_stlink_gui.py"
exit /b %ERRORLEVEL%
