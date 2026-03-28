# Cursor Current

Status: active
Audience: Cursor
Purpose: this is the live software-engineer execution entry in the external development-system workspace.

## Workspace anchors

- development-system workspace:
  - `D:\20251211\zhinengti\development-system\projects\houjinongfuai`
- business-code workspace:
  - `D:\20251211\zhinengti\houjinongfuai`
- frontend workspace:
  - `D:\20251211\zhinengti\lovable`

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
2. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/codex/CURRENT.md`
5. `docs/codex/WORK-MODES.md`
6. `docs/codex/TASK-TYPES.md`
7. `D:\20251211\zhinengti\development-system\shared\global-rules\task-taxonomy-matrix.md`
8. `D:\20251211\zhinengti\development-system\shared\global-rules\role-lane-catalog.md`
9. `D:\20251211\zhinengti\development-system\shared\global-rules\encoding-governance.md`
10. `D:\20251211\zhinengti\development-system\shared\global-rules\development-system-evolution-rule.md`
11. `D:\20251211\zhinengti\development-system\shared\global-rules\project-config-standard.md`
12. `D:\20251211\zhinengti\development-system\shared\global-rules\windows-path-governance.md`
13. `docs/governance/file-only-command-protocol.md`
14. `docs/governance/requirements-to-tasks-rule.md`
15. `docs/governance/requirements-engineering-standard.md`
16. `docs/governance/role-based-delivery-model.md`
17. `docs/governance/definition-of-ready.md`
18. `docs/governance/delivery-workflow.md`
19. `docs/governance/uat-execution-standard.md`
20. `docs/governance/uat-scenario-registry.md`
21. `docs/governance/device-simulation-registry.md`
22. `docs/governance/uat-problem-solving-map.md`
23. `docs/governance/current-wave-2026-03-24.md`
24. the active task file linked from `CURRENT.md`
25. `docs/codex/RESULT.md`

## Allowed working area

- development-system docs in this folder
- development-system helper scripts in `D:\20251211\zhinengti\development-system\projects\houjinongfuai\tools`
- business-code backend in `D:\20251211\zhinengti\houjinongfuai\backend`
- business-code docs in `D:\20251211\zhinengti\houjinongfuai\docs` when the active task requires them

## Path rule

- interpret `docs/codex/*` and `docs/governance/*` relative to this development-system workspace
- interpret backend and business-doc paths relative to `D:\20251211\zhinengti\houjinongfuai`
- for frontend `SYNC` or `VERIFY`, use `D:\20251211\zhinengti\lovable`

## Execute now

- Wait for PM to set `active task`.
- If `active task = none`, report `no active task` and stop.
- If `task type = none`, report `task type missing` and stop.
- Do not treat chat discussion, architecture discussion, governance discussion, or curiosity questions as executable work unless PM freezes them into the active task files.
- If a task reveals a useful process or tooling improvement, report it as a development-system improvement candidate instead of silently expanding scope.

## Hard constraints

- Do not reopen architecture.
- Do not modify frontend business code unless PM opens a new task.
- Backend-only batch unless PM re-opens `lovable` paths.

## Result writeback

After each execution, update `docs/codex/RESULT.md` using the fixed format in the protocol file.
