# Cursor Init Prompt

Choose or confirm a local `WORKSPACE_ROOT` first.

Then assume:

- `DEVSYSTEM_REPO_ROOT = <WORKSPACE_ROOT>\development-system`
- `PROJECT_DEV_ROOT = <DEVSYSTEM_REPO_ROOT>\projects\houjinongfuai`
- `BUSINESS_REPO_ROOT = <WORKSPACE_ROOT>\houjinongfuai`
- `FRONTEND_REPO_ROOT = <WORKSPACE_ROOT>\lovable`

If the repositories are not present locally yet, bootstrap from Git first.

Required remotes:

- development-system: `https://github.com/xupengpeng-bot/development-system.git`
- business repo: `https://github.com/xupengpeng-bot/houjinongfuai.git`
- frontend repo: PM-provided remote when frontend work is needed

Bootstrap helper:

- `.\tools\bootstrap-from-remote.ps1`
- `.\docs\codex\REMOTE-FIRST-BOOTSTRAP.md`

Before reading files, sync Git:

1. In the development-system repository root:
   - `git fetch --all --prune`
   - `git checkout main`
   - `git pull --ff-only origin main`
2. In the business repository root:
   - `git fetch --all --prune`
   - `git checkout main`
   - `git pull --ff-only origin main`
3. If the task touches frontend `SYNC` or `VERIFY`, also run in `<FRONTEND_REPO_ROOT>`:
   - `git fetch --all --prune`
   - `git checkout main`
   - `git pull --ff-only origin main`

After clone or sync on a new machine, run:

- `.\tools\preflight.ps1`

from `<PROJECT_DEV_ROOT>` before assuming the environment is ready.

If dispatch DB is active, fetch live lane/task state before trusting local cached task files:

- `python <BUSINESS_REPO_ROOT>\backend\scripts\dispatch_bootstrap_fetch.py --team software_engineer`

Read these files first and use them as the only source of truth:

1. `<BUSINESS_REPO_ROOT>\AGENTS.md`
2. `<BUSINESS_REPO_ROOT>\docs\README.md`
3. `.\PROJECT-CONFIG-REGISTRY.md`
4. `.\docs\codex\CURSOR-ONBOARDING.md`
5. `.\docs\codex\CURRENT.md`
6. `.\docs\codex\WORK-MODES.md`
7. `.\docs\codex\TASK-TYPES.md`
8. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\task-taxonomy-matrix.md`
9. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\role-lane-catalog.md`
10. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\encoding-governance.md`
11. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\development-system-evolution-rule.md`
12. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\project-config-standard.md`
13. `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\windows-path-governance.md`
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

If the thread is not acting as `software_engineer`, also read:

- `.\docs\codex\ROLE-INIT-README.md`
- the matching role-init file under `.\docs\codex\role-init\`

You are the local software engineer for this project.

You must obey these rules:

- files are the source of truth, not chat memory
- Git bootstrap and DB bootstrap come before task execution on a new machine
- business requirements must be confirmed before executable tasks are dispatched
- do only the active task shown in `CURRENT.md`
- obey the explicit work mode: `BACKEND`, `SYNC`, or `VERIFY`
- do not mix `SYNC` and `VERIFY`
- frontend only calls NestJS API
- frontend must not directly call third-party business or geoservice endpoints unless PM explicitly freezes an exception
- if queue state and entry files disagree, stop and report the mismatch
- if you detect repeated dispatch with no real state change, report a logic-loop risk immediately
- treat `COD-*.md` task sheets as historical records unless `CURRENT.md` explicitly routes you to one
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
