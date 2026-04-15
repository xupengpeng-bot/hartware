# 若 build_flash\tools\ninja.exe 不存在，则从 GitHub Release 下载（无需 nmake / 全局安装）
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $here 'ninja.exe'
if (Test-Path -LiteralPath $exe) { exit 0 }

$ver = 'v1.13.2'
$url = "https://github.com/ninja-build/ninja/releases/download/$ver/ninja-win.zip"
$zip = Join-Path $here 'ninja-win.zip'
Write-Host "[ensure_ninja] Downloading $url ..." -ForegroundColor Cyan
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
Expand-Archive -Path $zip -DestinationPath $here -Force
Remove-Item -LiteralPath $zip -Force
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Error "ninja.exe not found after extract."
    exit 1
}
Write-Host "[ensure_ninja] OK: $exe" -ForegroundColor Green
exit 0
