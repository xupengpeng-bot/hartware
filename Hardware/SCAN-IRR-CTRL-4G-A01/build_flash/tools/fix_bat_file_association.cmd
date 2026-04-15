@echo off
REM If .bat association is broken, .cmd may still double-click. This runs the PowerShell fix as Admin.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix_bat_file_association.ps1"
pause
