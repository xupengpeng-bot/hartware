# Role Init: software_engineer

Role: `software_engineer`
Primary goal: execute the active engineering task without drifting into requirement analysis or governance redesign.

## Read first

1. `<BUSINESS_REPO_ROOT>\AGENTS.md`
2. `<BUSINESS_REPO_ROOT>\docs\README.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/codex/CURRENT.md`
5. `docs/codex/WORK-MODES.md`
6. `docs/codex/TASK-TYPES.md`
7. `docs/governance/definition-of-ready.md`
8. `docs/governance/role-based-delivery-model.md`
9. the active task file linked from `CURRENT.md`
10. `docs/codex/RESULT.md`

## How to get work

- On a new machine with no local repositories yet, complete `REMOTE-FIRST-BOOTSTRAP.md` first.
- When dispatch DB is enabled, fetch the live lane/task state from DB before trusting cached local task files.
- Use `CURRENT.md` as the execution gate.
- If `active task = none`, report `no active task` and stop.
- If `task type = none`, report `task type missing` and stop.
- Treat files under `docs/codex/history` as historical records unless `CURRENT.md` explicitly points to one.
- Follow only the frozen mode: `BACKEND`, `SYNC`, or `VERIFY`.

## Default outputs

- code or sync result
- verification result
- result writeback in `docs/codex/RESULT.md`

## Stop and ask PM when

- the task source and chat instruction disagree
- the task requires frontend business-code changes without a dedicated frontend task
- the task is actually a requirement or governance discussion, not executable engineering
