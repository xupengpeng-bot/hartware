param(
    [Parameter(Mandatory = $true)]
    [string]$WorkspaceRoot,
    [string]$BusinessRepoUrl = 'https://github.com/xupengpeng-bot/houjinongfuai.git',
    [string]$DevSystemRepoUrl = 'https://github.com/xupengpeng-bot/development-system.git',
    [string]$FrontendRepoUrl = '',
    [switch]$SkipFrontend
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Ensure-Repo {
    param(
        [string]$TargetPath,
        [string]$RepoUrl
    )

    if ([string]::IsNullOrWhiteSpace($RepoUrl)) {
        return
    }

    if (Test-Path -LiteralPath (Join-Path $TargetPath '.git')) {
        git -C $TargetPath fetch --all --prune
        git -C $TargetPath checkout main
        git -C $TargetPath pull --ff-only origin main
        return
    }

    git clone $RepoUrl $TargetPath
    git -C $TargetPath checkout main
}

$WorkspaceRoot = (Resolve-Path -LiteralPath (New-Item -ItemType Directory -Force -Path $WorkspaceRoot)).Path
$BusinessRoot = Join-Path $WorkspaceRoot 'houjinongfuai'
$DevSystemRoot = Join-Path $WorkspaceRoot 'development-system'
$ProjectDevRoot = Join-Path $DevSystemRoot 'projects\houjinongfuai'
$FrontendRoot = Join-Path $WorkspaceRoot 'lovable'

Ensure-Repo -TargetPath $DevSystemRoot -RepoUrl $DevSystemRepoUrl
Ensure-Repo -TargetPath $BusinessRoot -RepoUrl $BusinessRepoUrl

if (-not $SkipFrontend -and -not [string]::IsNullOrWhiteSpace($FrontendRepoUrl)) {
    Ensure-Repo -TargetPath $FrontendRoot -RepoUrl $FrontendRepoUrl
}

Write-Host 'Bootstrap complete.'
Write-Host "WorkspaceRoot: $WorkspaceRoot"
Write-Host "BusinessRoot: $BusinessRoot"
Write-Host "DevSystemRoot: $DevSystemRoot"
Write-Host "ProjectDevRoot: $ProjectDevRoot"
Write-Host "FrontendRoot: $FrontendRoot"
Write-Host ''
Write-Host 'Next steps:'
Write-Host "1. cd `"$ProjectDevRoot`""
Write-Host "2. .\\tools\\preflight.ps1 -BusinessRoot `"$BusinessRoot`" -FrontendRoot `"$FrontendRoot`""
Write-Host "3. If dispatch DB is active: cd `"$BusinessRoot\\backend`" ; python .\\scripts\\dispatch_bootstrap_fetch.py --team software_engineer"
