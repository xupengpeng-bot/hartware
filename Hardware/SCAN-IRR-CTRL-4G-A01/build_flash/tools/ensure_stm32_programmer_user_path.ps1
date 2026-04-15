# 将 STM32CubeProgrammer 的 bin 目录追加到当前用户 PATH（若尚未存在），供新开的 CMD/IDE 自动可用
param(
    [Parameter(Mandatory = $true)]
    [string] $BinDir
)

$ErrorActionPreference = 'Stop'
$exe = Join-Path $BinDir 'STM32_Programmer_CLI.exe'
if (-not (Test-Path -LiteralPath $exe)) { exit 0 }

$bin = ([IO.Path]::GetFullPath($BinDir)).TrimEnd('\')
function Normalize-PathToken([string] $p) {
    if (-not $p) { return '' }
    return ([IO.Path]::GetFullPath($p.Trim())).TrimEnd('\').ToLowerInvariant()
}
$binKey = Normalize-PathToken $bin

$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if ($null -eq $userPath) { $userPath = '' }

$tokens = @()
foreach ($seg in $userPath -split ';') {
    $t = $seg.Trim()
    if ($t) { $tokens += $t }
}
foreach ($t in $tokens) {
    if ((Normalize-PathToken $t) -eq $binKey) { exit 0 }
}

$newPath = if ($userPath.Trim()) { "$userPath;$bin" } else { $bin }
[Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
exit 0
