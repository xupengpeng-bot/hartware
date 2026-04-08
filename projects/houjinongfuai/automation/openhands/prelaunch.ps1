param(
    [switch]$RequireActiveTask,
    [string]$BusinessRoot = $(Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))))) 'houjinongfuai'),
    [string]$FrontendRoot = $(Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))))) 'lovable')
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectRoot = Split-Path -Parent $ProjectRoot
$CurrentFile = Join-Path $ProjectRoot 'docs\codex\CURRENT.md'
$LaunchProfile = Join-Path $ProjectRoot 'automation\openhands\launch-profile.md'
$PromptFile = Join-Path $ProjectRoot 'automation\openhands\software-engineer-prompt.md'
$ResultFile = Join-Path $ProjectRoot 'docs\codex\RESULT.md'
$Checks = @()

function Add-Check {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Details
    )

    $script:Checks += [pscustomobject]@{
        Name    = $Name
        Status  = $Status
        Details = $Details
    }
}

function Test-RequiredPath {
    param(
        [string]$Name,
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Add-Check -Name $Name -Status 'PASS' -Details $Path
    }
    else {
        Add-Check -Name $Name -Status 'FAIL' -Details "missing: $Path"
    }
}

Test-RequiredPath -Name 'launch profile' -Path $LaunchProfile
Test-RequiredPath -Name 'software prompt' -Path $PromptFile
Test-RequiredPath -Name 'current file' -Path $CurrentFile
Test-RequiredPath -Name 'result file' -Path $ResultFile

if (Test-Path -LiteralPath $CurrentFile) {
    $currentContent = Get-Content -LiteralPath $CurrentFile -Raw
    $activeTask = [regex]::Match($currentContent, '(?ms)- active task\s*\r?\n\s*-\s*(.+?)\r?\n').Groups[1].Value.Trim()
    $taskType = [regex]::Match($currentContent, '(?ms)- task type\s*\r?\n\s*-\s*(.+?)\r?\n').Groups[1].Value.Trim()
    $mode = [regex]::Match($currentContent, '(?ms)## Work mode\s*\r?\n\r?\n-\s*(.+?)\r?\n').Groups[1].Value.Trim()

    if (-not $activeTask) {
        Add-Check -Name 'dispatch:active task' -Status 'FAIL' -Details 'CURRENT.md missing active task value'
    }
    elseif ($activeTask -eq 'none') {
        if ($RequireActiveTask) {
            Add-Check -Name 'dispatch:active task' -Status 'FAIL' -Details 'active task is none'
        }
        else {
            Add-Check -Name 'dispatch:active task' -Status 'WARN' -Details 'active task is none'
        }
    }
    else {
        Add-Check -Name 'dispatch:active task' -Status 'PASS' -Details $activeTask
    }

    if (-not $taskType) {
        Add-Check -Name 'dispatch:task type' -Status 'FAIL' -Details 'CURRENT.md missing task type value'
    }
    elseif ($taskType -eq 'none') {
        if ($RequireActiveTask) {
            Add-Check -Name 'dispatch:task type' -Status 'FAIL' -Details 'task type is none'
        }
        else {
            Add-Check -Name 'dispatch:task type' -Status 'WARN' -Details 'task type is none'
        }
    }
    else {
        Add-Check -Name 'dispatch:task type' -Status 'PASS' -Details $taskType
    }

    if (-not $mode) {
        Add-Check -Name 'dispatch:mode' -Status 'FAIL' -Details 'CURRENT.md missing mode value'
    }
    elseif ($mode -eq 'IDLE' -and $RequireActiveTask) {
        Add-Check -Name 'dispatch:mode' -Status 'FAIL' -Details 'mode is IDLE'
    }
    else {
        Add-Check -Name 'dispatch:mode' -Status 'PASS' -Details $mode
    }
}

$preflightCommand = Join-Path $ProjectRoot 'tools\preflight.ps1'
if (Test-Path -LiteralPath $preflightCommand) {
    & $preflightCommand -BusinessRoot $BusinessRoot -FrontendRoot $FrontendRoot | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Add-Check -Name 'preflight' -Status 'PASS' -Details 'preflight.ps1 passed'
    }
    else {
        Add-Check -Name 'preflight' -Status 'FAIL' -Details 'preflight.ps1 failed'
    }
}
else {
    Add-Check -Name 'preflight' -Status 'FAIL' -Details "missing: $preflightCommand"
}

$statusOrder = @{
    PASS = 0
    WARN = 1
    FAIL = 2
}

$sortedChecks = $Checks | Sort-Object @{ Expression = { $statusOrder[$_.Status] } }, Name
$sortedChecks | Format-Table -AutoSize

$passCount = ($Checks | Where-Object { $_.Status -eq 'PASS' } | Measure-Object).Count
$warnCount = ($Checks | Where-Object { $_.Status -eq 'WARN' } | Measure-Object).Count
$failCount = ($Checks | Where-Object { $_.Status -eq 'FAIL' } | Measure-Object).Count

Write-Host ""
Write-Host ("Summary: {0} pass, {1} warn, {2} fail" -f $passCount, $warnCount, $failCount)

if ($failCount -gt 0) {
    exit 1
}

if ($RequireActiveTask -and $warnCount -gt 0) {
    exit 1
}

exit 0
