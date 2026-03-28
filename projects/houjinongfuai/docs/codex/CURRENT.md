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
2. `D:\20251211\zhinengti\houjinongfuai\docs\系统说明\通用产品规则.md`
3. `docs/codex/CURRENT.md`
4. `docs/codex/WORK-MODES.md`
5. `docs/codex/TASK-TYPES.md`
6. `docs/governance/file-only-command-protocol.md`
7. `docs/governance/requirements-to-tasks-rule.md`
8. `docs/governance/definition-of-ready.md`
9. `docs/governance/delivery-workflow.md`
10. `docs/governance/current-wave-2026-03-24.md`
11. the active task file linked from `CURRENT.md`
12. `docs/codex/RESULT.md`

## Allowed working area

- development-system docs in this folder
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

## Hard constraints

- Do not reopen architecture.
- Do not modify frontend business code unless PM opens a new task.
- Backend-only batch unless PM re-opens `lovable` paths.

## Result writeback

After each execution, update `docs/codex/RESULT.md` using the fixed format in the protocol file.
