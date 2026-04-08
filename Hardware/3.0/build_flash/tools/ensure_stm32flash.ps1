# 与 3.0jijingoldplatform\tools\ensure_stm32flash.ps1 一致：优先 D:\Tools\stm32flash\
# Usage:
#   .\tools\ensure_stm32flash.ps1 -ResolveOnly   # 仅输出 stm32flash.exe 完整路径（供 BAT 调用）

param(
    [string]$InstallDir = 'D:\Tools\stm32flash',
    [switch]$ResolveOnly,
    [switch]$OpenDownloadPage
)

$ErrorActionPreference = 'Stop'
$exeName = 'stm32flash.exe'
$installDir = [IO.Path]::GetFullPath($InstallDir)
$preferred = Join-Path $installDir $exeName

function Find-Stm32FlashExe {
    $bundled = Join-Path $PSScriptRoot 'stm32flash-0.7-binaries\stm32flash.exe'
    if (Test-Path -LiteralPath $bundled) {
        return [IO.Path]::GetFullPath($bundled)
    }
    if (Test-Path -LiteralPath $preferred) {
        return [IO.Path]::GetFullPath($preferred)
    }
    $cmd = Get-Command $exeName -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) {
        return [IO.Path]::GetFullPath($cmd.Source)
    }
    return $null
}

$found = Find-Stm32FlashExe
if ($found) {
    if ($ResolveOnly) {
        Write-Output $found
    } else {
        Write-Host "stm32flash: $found"
    }
    exit 0
}

New-Item -ItemType Directory -Force -Path $installDir | Out-Null

$msg = @"
stm32flash.exe not found.

  Place the executable as:
    $preferred

  Or add $exeName to your PATH.

  Releases / source:
    https://sourceforge.net/projects/stm32flash/files/
"@

if ($ResolveOnly) {
    [Console]::Error.WriteLine($msg)
} else {
    Write-Host $msg
}

if ($OpenDownloadPage) {
    Start-Process 'https://sourceforge.net/projects/stm32flash/files/'
}

exit 1
