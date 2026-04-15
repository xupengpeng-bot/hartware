@echo off
chcp 65001 >nul
setlocal

call "%~dp0build.cmd"
if errorlevel 1 exit /b 1

call "%~dp0flash.cmd"
exit /b %ERRORLEVEL%
