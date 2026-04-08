# Cursor Current

Status: active
Audience: Cursor
Purpose: this is the live software-engineer execution entry in the external development-system workspace.

## Workspace anchors

Use workspace-relative anchors on the current machine:

- `WORKSPACE_ROOT`
- `DEVSYSTEM_REPO_ROOT = <WORKSPACE_ROOT>\development-system`
- `PROJECT_DEV_ROOT = <DEVSYSTEM_REPO_ROOT>\projects\houjinongfuai`
- `BUSINESS_REPO_ROOT = <WORKSPACE_ROOT>\houjinongfuai`
- `FRONTEND_REPO_ROOT = <WORKSPACE_ROOT>\lovable`

## Current state

- completed tasks
  - historical task records are kept under `docs/codex/history`
- active task
  - none
- task type
  - none

## Work mode

- IDLE

## Small read chain

1. `START-HERE.md`
2. `<BUSINESS_REPO_ROOT>\AGENTS.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `WORK-MODES.md`
5. `TASK-TYPES.md`
6. the active task file linked from `CURRENT.md`
7. `RESULT.md`

If the thread is role-specific:

- read `ROLE-INIT-README.md`
- then read the matching `role-init/*` card

If the domain is unclear:

- read `DOMAIN-NAVIGATION.md`

## Allowed working area

- development-system docs in this folder
- development-system helper scripts in `<PROJECT_DEV_ROOT>\tools`
- business-code backend in `<BUSINESS_REPO_ROOT>\backend`
- business-code docs in `<BUSINESS_REPO_ROOT>\docs` when the active task requires them

## Path rule

- interpret `docs/codex/*` and `docs/governance/*` relative to this development-system workspace
- interpret backend and business-doc paths relative to `<BUSINESS_REPO_ROOT>`
- for frontend `SYNC` or `VERIFY`, use `<FRONTEND_REPO_ROOT>`

## Execute now

- On a brand-new machine, complete Git bootstrap first. See `REMOTE-FIRST-BOOTSTRAP.md`.
- When dispatch DB is active, read the lane/task bootstrap from DB before trusting cached local task files.
- Wait for PM to set `active task`.
- If `active task = none`, report `no active task` and stop.
- If `task type = none`, report `task type missing` and stop.
- Treat files under `docs/codex/history` as historical records unless `CURRENT.md` points to one.
- Do not treat chat discussion, architecture discussion, governance discussion, or curiosity questions as executable work unless PM freezes them into the active task files.
- If a task reveals a useful process or tooling improvement, report it as a development-system improvement candidate instead of silently expanding scope.

## Hard constraints

- Do not reopen architecture.
- Do not modify frontend business code unless PM opens a new task.
- Backend-only batch unless PM re-opens `lovable` paths.

## Result writeback

After each execution, update `RESULT.md` using the fixed format in the protocol file.
