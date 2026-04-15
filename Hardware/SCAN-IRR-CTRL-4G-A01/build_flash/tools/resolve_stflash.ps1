# 输出 st-flash.exe 的完整路径（一行），供 flash.cmd 使用
param([string] $BuildFlashDir = '')

$ErrorActionPreference = 'SilentlyContinue'

function Try-Path([string] $p) {
    if (-not $p) { return $null }
    try {
        $f = [IO.Path]::GetFullPath($p)
        if (Test-Path -LiteralPath $f) { return $f }
    } catch {}
    return $null
}

if (-not $BuildFlashDir) {
    $BuildFlashDir = Split-Path -Parent $PSScriptRoot
}
$bf = [IO.Path]::GetFullPath($BuildFlashDir)

$rel = @(
    (Join-Path $bf 'tools\st-flash.exe'),
    (Join-Path $bf 'tools\stlink-1.8.0-win32\bin\st-flash.exe'),
    (Join-Path $bf '..\..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe'),
    (Join-Path $bf '..\..\skills\stlink-1.8.0-win32\bin\st-flash.exe')
)
foreach ($c in $rel) {
    $x = Try-Path $c
    if ($x) { Write-Output $x; exit 0 }
}

try {
    $agentRoot = [IO.Path]::GetFullPath((Join-Path $bf '..\..\..\'))
} catch {
    $agentRoot = $null
}

if ($agentRoot -and (Test-Path -LiteralPath $agentRoot)) {
    foreach ($sub in @('skills', 'tools')) {
        $base = Join-Path $agentRoot $sub
        if (-not (Test-Path -LiteralPath $base)) { continue }
        $hit = Get-ChildItem -Path $base -Filter 'st-flash.exe' -Recurse -File -Depth 8 -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($hit) {
            Write-Output $hit.FullName
            exit 0
        }
    }
}

exit 1
