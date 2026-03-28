param(
    [string]$BusinessRoot = 'D:\20251211\zhinengti\houjinongfuai',
    [string]$FrontendRoot = 'D:\20251211\zhinengti\lovable'
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectsRoot = Split-Path -Parent $ProjectRoot
$DevSystemRoot = Split-Path -Parent $ProjectsRoot
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

function Test-OptionalCommand {
    param(
        [string]$CommandName,
        [string]$Level
    )

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($command) {
        Add-Check -Name "tool:$CommandName" -Status 'PASS' -Details $command.Source
    }
    else {
        Add-Check -Name "tool:$CommandName" -Status $Level -Details 'not found in PATH'
    }
}

function Test-TextForReplacementChar {
    param(
        [string]$Name,
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        Add-Check -Name $Name -Status 'FAIL' -Details "missing: $Path"
        return
    }

    $content = Get-Content -LiteralPath $Path -Raw
    if ($content.IndexOf([char]0xFFFD) -ge 0) {
        Add-Check -Name $Name -Status 'FAIL' -Details 'replacement character detected'
    }
    else {
        Add-Check -Name $Name -Status 'PASS' -Details 'no replacement character detected'
    }
}

$CurrentFile = Join-Path $ProjectRoot 'docs\codex\CURRENT.md'
$ResultFile = Join-Path $ProjectRoot 'docs\codex\RESULT.md'
$TaskTypesFile = Join-Path $ProjectRoot 'docs\codex\TASK-TYPES.md'
$TaskDispatchTemplateFile = Join-Path $ProjectRoot 'docs\codex\TASK-DISPATCH-TEMPLATE.md'
$RoleInitReadme = Join-Path $ProjectRoot 'docs\codex\ROLE-INIT-README.md'
$RoleInitRoot = Join-Path $ProjectRoot 'docs\codex\role-init'
$ProjectConfigRegistry = Join-Path $ProjectRoot 'PROJECT-CONFIG-REGISTRY.md'
$ProjectMarketingBrief = Join-Path $ProjectRoot 'PROJECT-MARKETING-BRIEF.md'
$ReadyFile = Join-Path $ProjectRoot 'docs\governance\definition-of-ready.md'
$ReqRuleFile = Join-Path $ProjectRoot 'docs\governance\requirements-to-tasks-rule.md'
$ReqEngineeringFile = Join-Path $ProjectRoot 'docs\governance\requirements-engineering-standard.md'
$ReqChangeTemplateFile = Join-Path $ProjectRoot 'docs\governance\REQUIREMENT-CHANGE-TEMPLATE.md'
$MarketingStandardFile = Join-Path $ProjectRoot 'docs\governance\marketing-strategy-standard.md'
$MarketingTemplateFile = Join-Path $ProjectRoot 'docs\governance\MARKETING-MATERIAL-TEMPLATE.md'
$RoleBasedModelFile = Join-Path $ProjectRoot 'docs\governance\role-based-delivery-model.md'
$UatExecutionFile = Join-Path $ProjectRoot 'docs\governance\uat-execution-standard.md'
$UatEvidenceTemplateFile = Join-Path $ProjectRoot 'docs\governance\UAT-EVIDENCE-TEMPLATE.md'
$UatProblemMapFile = Join-Path $ProjectRoot 'docs\governance\uat-problem-solving-map.md'
$UatScenarioRegistryFile = Join-Path $ProjectRoot 'docs\governance\uat-scenario-registry.md'
$DeviceSimulationRegistryFile = Join-Path $ProjectRoot 'docs\governance\device-simulation-registry.md'
$BusinessAgents = Join-Path $BusinessRoot 'AGENTS.md'
$BusinessDocsReadme = Join-Path $BusinessRoot 'docs\README.md'
$BusinessRulesFile = Join-Path $BusinessRoot 'docs\README.md'
$SharedTaxonomy = Join-Path $DevSystemRoot 'shared\global-rules\task-taxonomy-matrix.md'
$SharedRoleCatalog = Join-Path $DevSystemRoot 'shared\global-rules\role-lane-catalog.md'
$SharedEncoding = Join-Path $DevSystemRoot 'shared\global-rules\encoding-governance.md'
$SharedEvolution = Join-Path $DevSystemRoot 'shared\global-rules\development-system-evolution-rule.md'
$SharedProjectConfigStandard = Join-Path $DevSystemRoot 'shared\global-rules\project-config-standard.md'
$SharedWindowsPath = Join-Path $DevSystemRoot 'shared\global-rules\windows-path-governance.md'

Test-RequiredPath -Name 'development-system project root' -Path $ProjectRoot
Test-RequiredPath -Name 'development-system root' -Path $DevSystemRoot
Test-RequiredPath -Name 'business workspace' -Path $BusinessRoot
if (Test-Path -LiteralPath $FrontendRoot) {
    Add-Check -Name 'frontend workspace' -Status 'PASS' -Details $FrontendRoot
}
else {
    Add-Check -Name 'frontend workspace' -Status 'WARN' -Details "missing: $FrontendRoot"
}

Test-RequiredPath -Name 'current file' -Path $CurrentFile
Test-RequiredPath -Name 'result file' -Path $ResultFile
Test-RequiredPath -Name 'task types file' -Path $TaskTypesFile
Test-RequiredPath -Name 'task dispatch template' -Path $TaskDispatchTemplateFile
Test-RequiredPath -Name 'role init readme' -Path $RoleInitReadme
Test-RequiredPath -Name 'role init root' -Path $RoleInitRoot
Test-RequiredPath -Name 'project config registry' -Path $ProjectConfigRegistry
Test-RequiredPath -Name 'project marketing brief' -Path $ProjectMarketingBrief
Test-RequiredPath -Name 'definition of ready file' -Path $ReadyFile
Test-RequiredPath -Name 'requirements-to-tasks file' -Path $ReqRuleFile
Test-RequiredPath -Name 'requirements-engineering standard' -Path $ReqEngineeringFile
Test-RequiredPath -Name 'requirement-change template' -Path $ReqChangeTemplateFile
Test-RequiredPath -Name 'marketing strategy standard' -Path $MarketingStandardFile
Test-RequiredPath -Name 'marketing material template' -Path $MarketingTemplateFile
Test-RequiredPath -Name 'role-based delivery model' -Path $RoleBasedModelFile
Test-RequiredPath -Name 'UAT execution standard' -Path $UatExecutionFile
Test-RequiredPath -Name 'UAT evidence template' -Path $UatEvidenceTemplateFile
Test-RequiredPath -Name 'UAT problem-solving map' -Path $UatProblemMapFile
Test-RequiredPath -Name 'UAT scenario registry' -Path $UatScenarioRegistryFile
Test-RequiredPath -Name 'device simulation registry' -Path $DeviceSimulationRegistryFile
Test-RequiredPath -Name 'business AGENTS' -Path $BusinessAgents
Test-RequiredPath -Name 'business docs README' -Path $BusinessDocsReadme
Test-RequiredPath -Name 'business product rules' -Path $BusinessRulesFile
Test-RequiredPath -Name 'shared taxonomy rule' -Path $SharedTaxonomy
Test-RequiredPath -Name 'shared role catalog' -Path $SharedRoleCatalog
Test-RequiredPath -Name 'shared encoding rule' -Path $SharedEncoding
Test-RequiredPath -Name 'shared evolution rule' -Path $SharedEvolution
Test-RequiredPath -Name 'shared project-config standard' -Path $SharedProjectConfigStandard
Test-RequiredPath -Name 'shared windows-path rule' -Path $SharedWindowsPath

Test-OptionalCommand -CommandName 'git' -Level 'FAIL'
Test-OptionalCommand -CommandName 'node' -Level 'WARN'
Test-OptionalCommand -CommandName 'npm' -Level 'WARN'
Test-OptionalCommand -CommandName 'python' -Level 'WARN'
Test-OptionalCommand -CommandName 'docker' -Level 'WARN'
Test-OptionalCommand -CommandName 'arm-none-eabi-gcc' -Level 'WARN'
Test-OptionalCommand -CommandName 'openocd' -Level 'WARN'

Test-TextForReplacementChar -Name 'encoding:CURRENT.md' -Path $CurrentFile
Test-TextForReplacementChar -Name 'encoding:RESULT.md' -Path $ResultFile
Test-TextForReplacementChar -Name 'encoding:definition-of-ready' -Path $ReadyFile
Test-TextForReplacementChar -Name 'encoding:requirements-to-tasks' -Path $ReqRuleFile
Test-TextForReplacementChar -Name 'encoding:requirements-engineering-standard' -Path $ReqEngineeringFile
Test-TextForReplacementChar -Name 'encoding:marketing-strategy-standard' -Path $MarketingStandardFile
Test-TextForReplacementChar -Name 'encoding:project-marketing-brief' -Path $ProjectMarketingBrief
Test-TextForReplacementChar -Name 'encoding:business AGENTS' -Path $BusinessAgents
Test-TextForReplacementChar -Name 'encoding:business product rules' -Path $BusinessRulesFile

if (Test-Path -LiteralPath $CurrentFile) {
    $currentContent = Get-Content -LiteralPath $CurrentFile -Raw
    $activeTask = [regex]::Match($currentContent, '(?ms)- active task\s*\r?\n\s*-\s*(.+?)\r?\n').Groups[1].Value.Trim()
    $taskType = [regex]::Match($currentContent, '(?ms)- task type\s*\r?\n\s*-\s*(.+?)\r?\n').Groups[1].Value.Trim()
    $mode = [regex]::Match($currentContent, '(?ms)## Work mode\s*\r?\n\r?\n-\s*(.+?)\r?\n').Groups[1].Value.Trim()

    if (-not $activeTask) {
        Add-Check -Name 'dispatch:active task field' -Status 'FAIL' -Details 'CURRENT.md missing active task value'
    }
    else {
        Add-Check -Name 'dispatch:active task field' -Status 'PASS' -Details $activeTask
    }

    if (-not $taskType) {
        Add-Check -Name 'dispatch:task type field' -Status 'FAIL' -Details 'CURRENT.md missing task type value'
    }
    else {
        Add-Check -Name 'dispatch:task type field' -Status 'PASS' -Details $taskType
    }

    if (-not $mode) {
        Add-Check -Name 'dispatch:mode field' -Status 'FAIL' -Details 'CURRENT.md missing mode value'
    }
    else {
        Add-Check -Name 'dispatch:mode field' -Status 'PASS' -Details $mode
    }

    if ($activeTask -eq 'none') {
        if ($taskType -eq 'none' -and $mode -eq 'IDLE') {
            Add-Check -Name 'dispatch:idle consistency' -Status 'PASS' -Details 'idle state is internally consistent'
        }
        else {
            Add-Check -Name 'dispatch:idle consistency' -Status 'WARN' -Details 'active task is none but task type or mode is not the expected idle value'
        }
    }
    elseif ($taskType -eq 'none') {
        Add-Check -Name 'dispatch:executable consistency' -Status 'FAIL' -Details 'active task exists but task type is none'
    }
    else {
        Add-Check -Name 'dispatch:executable consistency' -Status 'PASS' -Details 'active task has task type'
    }
}

$gitCommand = Get-Command git -ErrorAction SilentlyContinue
if ($gitCommand) {
    $devLongPaths = git -C $DevSystemRoot config --get core.longpaths
    if ($devLongPaths -eq 'true') {
        Add-Check -Name 'git:development-system longpaths' -Status 'PASS' -Details 'core.longpaths=true'
    }
    else {
        Add-Check -Name 'git:development-system longpaths' -Status 'WARN' -Details 'core.longpaths is not explicitly true'
    }

    $businessLongPaths = git -C $BusinessRoot config --get core.longpaths
    if ($businessLongPaths -eq 'true') {
        Add-Check -Name 'git:business longpaths' -Status 'PASS' -Details 'core.longpaths=true'
    }
    else {
        Add-Check -Name 'git:business longpaths' -Status 'WARN' -Details 'core.longpaths is not explicitly true'
    }

    $devSystemRemote = git -C $DevSystemRoot remote
    if ([string]::IsNullOrWhiteSpace(($devSystemRemote | Out-String))) {
        Add-Check -Name 'git:development-system remote' -Status 'WARN' -Details 'no remote configured'
    }
    else {
        Add-Check -Name 'git:development-system remote' -Status 'PASS' -Details (($devSystemRemote -join ', ').Trim())
    }

    $businessStatus = git -C $BusinessRoot status --short
    if ([string]::IsNullOrWhiteSpace(($businessStatus | Out-String))) {
        Add-Check -Name 'git:business workspace clean' -Status 'PASS' -Details 'working tree clean'
    }
    else {
        Add-Check -Name 'git:business workspace clean' -Status 'WARN' -Details 'working tree not clean'
    }

    $devStatus = git -C $DevSystemRoot status --short
    if ([string]::IsNullOrWhiteSpace(($devStatus | Out-String))) {
        Add-Check -Name 'git:development-system clean' -Status 'PASS' -Details 'working tree clean'
    }
    else {
        Add-Check -Name 'git:development-system clean' -Status 'WARN' -Details 'working tree not clean'
    }
}

$longPathsEnabled = (Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -ErrorAction SilentlyContinue).LongPathsEnabled
if ($longPathsEnabled -eq 1) {
    Add-Check -Name 'windows:long paths enabled' -Status 'PASS' -Details 'LongPathsEnabled=1'
}
else {
    Add-Check -Name 'windows:long paths enabled' -Status 'WARN' -Details 'LongPathsEnabled is not enabled'
}

function Add-PathLengthCheck {
    param(
        [string]$Name,
        [string]$RootPath
    )

    if (-not (Test-Path -LiteralPath $RootPath)) {
        Add-Check -Name $Name -Status 'WARN' -Details "root missing: $RootPath"
        return
    }

    $files = Get-ChildItem -LiteralPath $RootPath -Recurse -File -ErrorAction SilentlyContinue
    if (-not $files) {
        Add-Check -Name $Name -Status 'WARN' -Details 'no files found for path-length scan'
        return
    }

    $maxLength = ($files | ForEach-Object { $_.FullName.Length } | Measure-Object -Maximum).Maximum
    if ($maxLength -ge 220) {
        Add-Check -Name $Name -Status 'WARN' -Details "max path length=$maxLength"
    }
    else {
        Add-Check -Name $Name -Status 'PASS' -Details "max path length=$maxLength"
    }
}

Add-PathLengthCheck -Name 'paths:business workspace max length' -RootPath $BusinessRoot
Add-PathLengthCheck -Name 'paths:development-system max length' -RootPath $DevSystemRoot

$statusOrder = @{
    PASS = 0
    WARN = 1
    FAIL = 2
}

$sortedChecks = $Checks | Sort-Object @{ Expression = { $statusOrder[$_.Status] } }, Name
$sortedChecks | Format-Table -AutoSize

$passCount = ($Checks | Where-Object { $_.Status -eq 'PASS' } | Measure-Object).Count
$failCount = ($Checks | Where-Object { $_.Status -eq 'FAIL' } | Measure-Object).Count
$warningCount = ($Checks | Where-Object { $_.Status -eq 'WARN' } | Measure-Object).Count

Write-Host ""
Write-Host ("Summary: {0} pass, {1} warn, {2} fail" -f $passCount, $warningCount, $failCount)

if ($failCount -gt 0) {
    exit 1
}

exit 0
