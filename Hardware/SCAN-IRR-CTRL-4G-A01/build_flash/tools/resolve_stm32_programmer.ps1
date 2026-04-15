# 解析 STM32CubeProgrammer 的 bin 目录并输出一行路径（供 env_tools.cmd 加入 PATH）
# 环境变量 STM32_CUBE_PROGRAMMER_BIN 可强制指定 bin 目录

param(
    [string] $BuildFlashDir = ''
)

$ErrorActionPreference = 'SilentlyContinue'

function Test-BinDir([string] $dir) {
    if (-not $dir) { return $null }
    $exe = Join-Path $dir 'STM32_Programmer_CLI.exe'
    if (Test-Path -LiteralPath $exe) { return (Resolve-Path $dir).Path }
    return $null
}

if ($env:STM32_CUBE_PROGRAMMER_BIN) {
    $f = Test-BinDir $env:STM32_CUBE_PROGRAMMER_BIN
    if ($f) { Write-Output $f; exit 0 }
}

$f = Test-BinDir "${env:ProgramFiles}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin"
if ($f) { Write-Output $f; exit 0 }
$pf86 = ${env:ProgramFiles(x86)}
if ($pf86) {
    $f = Test-BinDir (Join-Path $pf86 'STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin')
    if ($f) { Write-Output $f; exit 0 }
}
# 用户级安装（无管理员权限时常装在此）
if ($env:LOCALAPPDATA) {
    foreach ($rel in @(
            'Programs\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin',
            'STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin'
        )) {
        $f = Test-BinDir (Join-Path $env:LOCALAPPDATA $rel)
        if ($f) { Write-Output $f; exit 0 }
    }
}
$f = Test-BinDir 'D:\STM32CubeProgrammer\bin'
if ($f) { Write-Output $f; exit 0 }
$f = Test-BinDir 'D:\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin'
if ($f) { Write-Output $f; exit 0 }

if (-not $BuildFlashDir) {
    $BuildFlashDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$bf = [IO.Path]::GetFullPath($BuildFlashDir)

$relCandidates = @(
    (Join-Path $bf 'tools\STM32CubeProgrammer\bin'),
    (Join-Path $bf '..\tools\STM32CubeProgrammer\bin'),
    (Join-Path $bf '..\..\tools\STM32CubeProgrammer\bin'),
    (Join-Path $bf '..\..\..\tools\STM32CubeProgrammer\bin'),
    (Join-Path $bf '..\STM32CubeProgrammer\bin'),
    (Join-Path $bf '..\..\STM32CubeProgrammer\bin'),
    (Join-Path $bf '..\..\..\STM32CubeProgrammer\bin')
)
foreach ($c in $relCandidates) {
    try {
        $full = [IO.Path]::GetFullPath($c)
        $f = Test-BinDir $full
        if ($f) { Write-Output $f; exit 0 }
    } catch {}
}

# 智能体根目录 = build_flash 向上 3 级（build_flash -> new -> hardware -> 智能体）
try {
    $agentRoot = [IO.Path]::GetFullPath((Join-Path $bf '..\..\..\'))
} catch {
    $agentRoot = $null
}

if ($agentRoot -and (Test-Path -LiteralPath $agentRoot)) {
    foreach ($sr in @(
            (Join-Path $agentRoot 'tools'),
            (Join-Path $agentRoot 'STM32CubeProgrammer'),
            (Join-Path $agentRoot 'hardware')
        )) {
        if (-not (Test-Path -LiteralPath $sr)) { continue }
        $hit = Get-ChildItem -Path $sr -Filter 'STM32_Programmer_CLI.exe' -Recurse -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($hit) {
            Write-Output $hit.DirectoryName
            exit 0
        }
    }
    # 最后在「智能体」根目录下有限深度全盘找（便携解压位置不固定时）
    $hit = Get-ChildItem -Path $agentRoot -Filter 'STM32_Programmer_CLI.exe' -Recurse -File -Depth 10 -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($hit) {
        Write-Output $hit.DirectoryName
        exit 0
    }
}

exit 1
