# 修复 .bat 无默认打开方式（恢复由 cmd.exe 执行）
# 若 .bat 无法双击：Win+R 输入 powershell 回车，执行:
#   Set-ExecutionPolicy -Scope Process Bypass -Force; cd "此脚本所在目录"; .\fix_bat_file_association.ps1

$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host 'Requesting Administrator...' -ForegroundColor Cyan
    Start-Process powershell.exe -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`""
    ) -Verb RunAs
    exit 0
}

$regFile = Join-Path $PSScriptRoot 'fix_bat_file_association.reg'
if (-not (Test-Path -LiteralPath $regFile)) {
    Write-Error "Missing: $regFile"
    exit 1
}

Write-Host 'Importing registry...' -ForegroundColor Cyan
& reg.exe import $regFile
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

# 部分系统需删除用户层「打开方式」覆盖，合并 .reg 后才会生效
$batExt = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.bat\UserChoice'
if (Test-Path -LiteralPath $batExt) {
    try {
        Remove-Item -LiteralPath $batExt -Recurse -Force
        Write-Host 'Cleared HKCU FileExts\.bat\UserChoice' -ForegroundColor Yellow
    } catch {
        Write-Host "Note: could not remove UserChoice: $_" -ForegroundColor Yellow
    }
}

Write-Host 'Done. Double-click a .bat to test. If needed, sign out or reboot.' -ForegroundColor Green
exit 0
