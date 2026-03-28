# OpenHands Launch Profile

Status: active
Audience: PM, development_system_engineer, software_engineer
Purpose: define the minimum launch contract when OpenHands is used as the execution runtime for this project.

## Position

OpenHands is an execution runtime.

It is not the project's truth source.

Truth still comes from:

1. `<BUSINESS_REPO_ROOT>`
2. `<PROJECT_DEV_ROOT>`
3. `CURRENT.md` now, and later dispatch DB when promoted

## Use this profile for the first pilot

Pilot scope:

- role: `software_engineer`
- task type: `ENGINEERING`
- mode: `BACKEND`

## Launch checklist

Before launch, confirm all of the following:

1. Git bootstrap completed
2. `preflight.ps1` passed
3. `CURRENT.md` is present
4. `CURRENT.md` contains:
   - active task
   - task type
   - mode
5. the active task file exists
6. the task has:
   - requirement source
   - allowed working area
   - verification basis
   - next handoff target

If any item is missing, do not launch execution.

## Required read order

OpenHands should read in this order:

1. `docs/codex/START-HERE.md`
2. `<BUSINESS_REPO_ROOT>\AGENTS.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/codex/CURRENT.md`
5. `docs/codex/WORK-MODES.md`
6. `docs/codex/TASK-TYPES.md`
7. the active task file
8. `docs/codex/RESULT.md`

Do not start by scanning historical task sheets.

## Historical rule

Files under `docs/codex/history` are archive records.

OpenHands should not use them as current task truth unless `CURRENT.md` explicitly routes there.

## Stop conditions

OpenHands should stop and report instead of guessing when:

1. `active task = none`
2. `task type = none`
3. mode is missing
4. allowed working area is missing
5. verification basis is missing
6. the request is still fuzzy and clearly belongs to requirement analysis first

## Output contract

Every run should end with:

1. task id
2. mode
3. status
4. changed files or synced files
5. verification result
6. commit SHA or `no git action`
7. pending issues
8. next handoff target
