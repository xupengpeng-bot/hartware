# Remote-First Bootstrap

Status: active
Audience: PM, Codex, Cursor, AI agents
Purpose: define how a brand-new machine should start when no local workspace, directory, or cached files exist yet.

## Core rule

On a brand-new machine:

- do not assume any workspace path already exists
- do not assume any repository has already been cloned
- do not assume any cached task file is still correct

The startup order must be:

1. choose a local workspace root
2. clone or pull the required Git repositories
3. read bootstrap state from dispatch DB when DB-backed dispatch is enabled
4. then enter the role-specific read chain

## Recommended workspace variables

Choose any local root that is short and Windows-friendly, for example:

- `WORKSPACE_ROOT`

Then treat these as derived paths:

- `BUSINESS_REPO_ROOT = <WORKSPACE_ROOT>\\houjinongfuai`
- `DEVSYSTEM_REPO_ROOT = <WORKSPACE_ROOT>\\development-system`
- `PROJECT_DEV_ROOT = <DEVSYSTEM_REPO_ROOT>\\projects\\houjinongfuai`
- `FRONTEND_REPO_ROOT = <WORKSPACE_ROOT>\\lovable`

The exact drive letter or parent directory may vary by machine.

## Required Git remotes

- business repository:
  - `https://github.com/xupengpeng-bot/houjinongfuai.git`
- development-system repository:
  - `https://github.com/xupengpeng-bot/development-system.git`
- frontend repository:
  - use the PM-provided remote when frontend work is required

## Bootstrap commands

### 1. Create a local workspace root

```powershell
$env:WORKSPACE_ROOT = 'D:\work\zhinengti'
New-Item -ItemType Directory -Force -Path $env:WORKSPACE_ROOT | Out-Null
```

### 2. Clone missing repositories

```powershell
cd $env:WORKSPACE_ROOT
git clone https://github.com/xupengpeng-bot/development-system.git
git clone https://github.com/xupengpeng-bot/houjinongfuai.git
```

If frontend work is needed, also clone the frontend repo into:

```text
<WORKSPACE_ROOT>\lovable
```

### 3. Sync to the latest main

```powershell
git -C "$env:WORKSPACE_ROOT\\development-system" fetch --all --prune
git -C "$env:WORKSPACE_ROOT\\development-system" checkout main
git -C "$env:WORKSPACE_ROOT\\development-system" pull --ff-only origin main

git -C "$env:WORKSPACE_ROOT\\houjinongfuai" fetch --all --prune
git -C "$env:WORKSPACE_ROOT\\houjinongfuai" checkout main
git -C "$env:WORKSPACE_ROOT\\houjinongfuai" pull --ff-only origin main
```

### 4. Run preflight after clone

```powershell
cd "$env:WORKSPACE_ROOT\\development-system\\projects\\houjinongfuai"
.\tools\preflight.ps1 -BusinessRoot "$env:WORKSPACE_ROOT\\houjinongfuai" -FrontendRoot "$env:WORKSPACE_ROOT\\lovable"
```

### 5. Read DB bootstrap when dispatch DB is active

Preferred helper:

```powershell
cd "$env:WORKSPACE_ROOT\\houjinongfuai\\backend"
python .\scripts\dispatch_bootstrap_fetch.py --team software_engineer
```

DB bootstrap should provide:

- active team lane
- active task id
- task type
- mode
- status
- optional execute-now summary

## Execution gate

- If DB bootstrap returns `active task = none`, report `no active task` and stop.
- If DB bootstrap does not expose an explicit `task_type`, stop.
- If DB bootstrap and local `CURRENT.md` disagree, report the mismatch and wait for PM clarification.

## Role handoff after bootstrap

After Git and DB bootstrap complete:

- `software_engineer` reads `CURRENT.md` and the linked task
- other roles read `ROLE-INIT-README.md` and their matching role-init card

## New-environment device rule

If the task touches embedded or hardware work on a new machine, do not proceed from remembered paths.

First ask PM / user to confirm the actual tool paths for that machine.
