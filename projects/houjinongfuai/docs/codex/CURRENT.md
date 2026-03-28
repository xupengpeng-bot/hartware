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
  - historical task records are kept in sibling task sheets in this folder
- active task
  - none
- task type
  - none

## Work mode

- IDLE

## Read order

1. `D:\20251211\zhinengti\houjinongfuai\AGENTS.md`
2. `<BUSINESS_REPO_ROOT>\docs\README.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/codex/CURRENT.md`
5. `docs/codex/WORK-MODES.md`
6. `docs/codex/TASK-TYPES.md`
7. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\task-taxonomy-matrix.md`
8. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\role-lane-catalog.md`
9. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\encoding-governance.md`
10. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\development-system-evolution-rule.md`
11. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\project-config-standard.md`
12. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\windows-path-governance.md`
13. `docs/governance/file-only-command-protocol.md`
14. `docs/governance/requirements-to-tasks-rule.md`
15. `docs/governance/requirements-engineering-standard.md`
16. `docs/governance/requirement-change-impact-standard.md`
17. `docs/governance/role-based-delivery-model.md`
18. `docs/governance/definition-of-ready.md`
19. `docs/governance/delivery-workflow.md`
20. `docs/governance/uat-execution-standard.md`
21. `docs/governance/uat-scenario-registry.md`
22. `docs/governance/device-simulation-registry.md`
23. `docs/governance/uat-problem-solving-map.md`
24. `docs/governance/current-wave-2026-03-24.md`
25. the active task file linked from `CURRENT.md`
26. `docs/codex/RESULT.md`

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

- On a brand-new machine, complete Git bootstrap first. See `docs/codex/REMOTE-FIRST-BOOTSTRAP.md`.
- When dispatch DB is active, read the lane/task bootstrap from DB before trusting cached local task files.
- Wait for PM to set `active task`.
- If `active task = none`, report `no active task` and stop.
- If `task type = none`, report `task type missing` and stop.
- Treat `COD-*.md` files in this folder as historical records unless `CURRENT.md` points to one.
- Do not treat chat discussion, architecture discussion, governance discussion, or curiosity questions as executable work unless PM freezes them into the active task files.
- If a task reveals a useful process or tooling improvement, report it as a development-system improvement candidate instead of silently expanding scope.

## Hard constraints

- Do not reopen architecture.
- Do not modify frontend business code unless PM opens a new task.
- Backend-only batch unless PM re-opens `lovable` paths.

## Result writeback

After each execution, update `docs/codex/RESULT.md` using the fixed format in the protocol file.
