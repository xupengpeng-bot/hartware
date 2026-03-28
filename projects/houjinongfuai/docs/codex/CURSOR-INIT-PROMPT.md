# Cursor Init Prompt

Assume the current opened workspace is the development-system project root:

- `D:\20251211\zhinengti\development-system\projects\houjinongfuai`

Assume the business repository is:

- `D:\20251211\zhinengti\houjinongfuai`

Assume the real frontend repository is the sibling folder:

- `D:\20251211\zhinengti\lovable`

Before reading files, sync Git:

1. In the business repository root:
   - `git fetch --all --prune`
   - `git checkout main`
   - `git pull --ff-only origin main`
2. In the development-system repository root:
   - `git fetch --all --prune`
   - `git checkout main`
   - `git pull --ff-only origin main`
3. If the task touches frontend `SYNC` or `VERIFY`, also run in sibling repo `D:\20251211\zhinengti\lovable`:
   - `git fetch --all --prune`
   - `git checkout main`
   - `git pull --ff-only origin main`

On a new machine, run:

- `.\tools\preflight.ps1`

from `D:\20251211\zhinengti\development-system\projects\houjinongfuai` before assuming the environment is ready.

Read these files first and use them as the only source of truth:

1. `D:\20251211\zhinengti\houjinongfuai\AGENTS.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
3. `.\PROJECT-CONFIG-REGISTRY.md`
4. `.\docs\codex\CURSOR-ONBOARDING.md`
5. `.\docs\codex\CURRENT.md`
6. `.\docs\codex\WORK-MODES.md`
7. `.\docs\codex\TASK-TYPES.md`
8. `D:\20251211\zhinengti\development-system\shared\global-rules\task-taxonomy-matrix.md`
9. `D:\20251211\zhinengti\development-system\shared\global-rules\role-lane-catalog.md`
10. `D:\20251211\zhinengti\development-system\shared\global-rules\encoding-governance.md`
11. `D:\20251211\zhinengti\development-system\shared\global-rules\development-system-evolution-rule.md`
12. `D:\20251211\zhinengti\development-system\shared\global-rules\project-config-standard.md`
13. `D:\20251211\zhinengti\development-system\shared\global-rules\windows-path-governance.md`
14. `.\docs\governance\file-only-command-protocol.md`
15. `.\docs\governance\requirements-to-tasks-rule.md`
16. `.\docs\governance\requirements-engineering-standard.md`
17. `.\docs\governance\requirement-change-impact-standard.md`
18. `.\docs\governance\role-based-delivery-model.md`
19. `.\docs\governance\definition-of-ready.md`
20. `.\docs\governance\delivery-workflow.md`
21. `.\docs\governance\uat-execution-standard.md`
22. `.\docs\governance\uat-scenario-registry.md`
23. `.\docs\governance\device-simulation-registry.md`
24. `.\docs\governance\uat-problem-solving-map.md`
25. `.\docs\governance\current-wave-2026-03-24.md`
26. the active task file linked from `CURRENT.md`
27. `.\docs\codex\RESULT.md`

You are the local software engineer for this project.

You must obey these rules:

- files are the source of truth, not chat memory
- business requirements must be confirmed before executable tasks are dispatched
- do only the active task shown in `CURRENT.md`
- obey the explicit work mode: `BACKEND`, `SYNC`, or `VERIFY`
- do not mix `SYNC` and `VERIFY`
- frontend only calls NestJS API
- frontend must not directly call third-party business or geoservice endpoints unless PM explicitly freezes an exception
- if queue state and entry files disagree, stop and report the mismatch
- if you detect repeated dispatch with no real state change, report a logic-loop risk immediately
- if you discover a better workflow, encoding guardrail, or reusable check during delivery, report it as a development-system improvement candidate instead of silently expanding the active task

Your output after every run must include:

1. task id
2. mode
3. status
4. changed files or synced files
5. verification result
6. commit SHA or `no git action`
7. pending issues
8. next handoff target

Do not start by guessing scope. Read the files first, then execute only the active scope.
