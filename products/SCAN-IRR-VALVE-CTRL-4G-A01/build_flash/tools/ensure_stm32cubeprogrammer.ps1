# 将 STM32CubeProgrammer 安装到本仓库 build_flash\tools\STM32CubeProgrammer（含 bin\STM32_Programmer_CLI.exe）
# ST 软件需从官网自行下载，本脚本不负责联网直链下载（许可与链接不稳定）。
# 用法:
#   powershell -ExecutionPolicy Bypass -File ensure_stm32cubeprogrammer.ps1 -ZipPath "C:\Users\你\Downloads\en.stm32cubeprog.zip"
#   powershell -ExecutionPolicy Bypass -File ensure_stm32cubeprogrammer.ps1 -SetupExe "D:\tmp\SetupSTM32CubeProgrammer_win64.exe"

param(
    [string] $ZipPath = '',
    [string] $SetupExe = ''
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$destRoot = Join-Path $here 'STM32CubeProgrammer'
$wantBin = Join-Path $destRoot 'bin\STM32_Programmer_CLI.exe'

if (Test-Path -LiteralPath $wantBin) {
    Write-Host "[ensure_stm32cubeprogrammer] 已存在: $wantBin" -ForegroundColor Green
    exit 0
}

function Find-FileRecursive([string] $root, [string] $name) {
    Get-ChildItem -LiteralPath $root -Recurse -Filter $name -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

function Install-FromSetup([string] $setupPath) {
    if (-not (Test-Path -LiteralPath $setupPath)) {
        throw "找不到安装包: $setupPath"
    }
    $dest = [IO.Path]::GetFullPath($destRoot)
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    # Inno Setup / 常见静默参数（若失败请用手动安装并选目录为 tools\STM32CubeProgrammer）
    $args = @(
        '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
        "/DIR=`"$dest`""
    )
    Write-Host "[ensure_stm32cubeprogrammer] 运行安装包: $setupPath" -ForegroundColor Cyan
    $p = Start-Process -FilePath $setupPath -ArgumentList $args -PassThru -Wait
    if ($p.ExitCode -ne 0) {
        Write-Host "[ensure_stm32cubeprogrammer] 静默安装返回 $($p.ExitCode)，尝试 NSIS 风格 /S ..." -ForegroundColor Yellow
        $p2 = Start-Process -FilePath $setupPath -ArgumentList @('/S', "/D=$dest") -PassThru -Wait
        if ($p2.ExitCode -ne 0) {
            throw "静默安装失败。请手动运行安装程序，目标目录选: $dest"
        }
    }
}

if ($SetupExe) {
    Install-FromSetup $SetupExe
    if (-not (Test-Path -LiteralPath $wantBin)) {
        throw "安装后仍未找到: $wantBin"
    }
    Write-Host "[ensure_stm32cubeprogrammer] OK: $wantBin" -ForegroundColor Green
    exit 0
}

if (-not $ZipPath) {
    Write-Host @'
[ensure_stm32cubeprogrammer] 未指定 ZipPath/SetupExe，且本地尚未安装。

请从 ST 官网下载 STM32CubeProgrammer 的 Windows 包（zip 内含 SetupSTM32CubeProgrammer_win64.exe），然后执行例如:

  powershell -ExecutionPolicy Bypass -File tools\ensure_stm32cubeprogrammer.ps1 -ZipPath "你的\en.stm32cubeprog.zip"

或直接指定已解压的安装程序:

  powershell -ExecutionPolicy Bypass -File tools\ensure_stm32cubeprogrammer.ps1 -SetupExe "D:\SetupSTM32CubeProgrammer_win64.exe"

安装目标请选择或静默解压到:
  build_flash\tools\STM32CubeProgrammer
'@
    exit 1
}

if (-not (Test-Path -LiteralPath $ZipPath)) {
    throw "找不到 zip: $ZipPath"
}

$tmp = Join-Path $env:TEMP ("stm32cubeprog_extract_" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
try {
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $tmp -Force
    $setup = Find-FileRecursive $tmp 'SetupSTM32CubeProgrammer_win64.exe'
    if (-not $setup) {
        $setup = Get-ChildItem -Path $tmp -Recurse -Filter 'SetupSTM32CubeProgrammer*.exe' -File -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $setup) {
        throw "zip 内未找到 SetupSTM32CubeProgrammer_win64.exe，请换用 -SetupExe 指向安装程序。"
    }
    Install-FromSetup $setup
} finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $wantBin)) {
    throw "完成后仍未找到: $wantBin（若安装到了 Program Files，可把其中 bin 复制到 tools\STM32CubeProgrammer\bin）"
}

Write-Host "[ensure_stm32cubeprogrammer] OK: $wantBin" -ForegroundColor Green
exit 0
