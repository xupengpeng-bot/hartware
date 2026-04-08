# Houjinongfuai Development System

Status: active
Audience: PM, software engineer, Cursor, Codex, Lovable
Purpose: serve as the external development-system workspace for the `houjinongfuai` project.

## Workspace split

Use workspace-relative anchors instead of machine-specific absolute paths:

- `WORKSPACE_ROOT`
- `DEVSYSTEM_REPO_ROOT = <WORKSPACE_ROOT>\development-system`
- `PROJECT_DEV_ROOT = <DEVSYSTEM_REPO_ROOT>\projects\houjinongfuai`
- `BUSINESS_REPO_ROOT = <WORKSPACE_ROOT>\houjinongfuai`
- `FRONTEND_REPO_ROOT = <WORKSPACE_ROOT>\lovable`

The exact drive letter or parent directory may differ by machine.

## Read-first rule

When starting work for this project:

1. read business boundaries from `<BUSINESS_REPO_ROOT>\AGENTS.md`
2. read `PROJECT-CONFIG-REGISTRY.md` when environment, startup, or verification is involved
3. read dispatch and workflow docs in this development-system workspace
4. execute only the active typed task

## New-machine bootstrap rule

On a new machine with no local workspace yet:

1. bootstrap from Git first
2. fetch live task state from dispatch DB when DB-backed dispatch is enabled
3. then enter the normal role read chain

Bootstrap guide:

- `docs/codex/REMOTE-FIRST-BOOTSTRAP.md`
- `tools/bootstrap-from-remote.ps1`

## Cross-device note

On a new machine, run:

- `.\tools\preflight.ps1`

after the repositories have been cloned and synced.
