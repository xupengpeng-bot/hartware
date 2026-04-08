# Codex Dispatch 2026-03-24

Status: active
Audience: Codex local backend developer
Purpose: official PM dispatch for the current backend wave.

## Read first

1. `AGENTS.md`
2. `docs/README.md`
3. `docs/governance/delivery-workflow.md`
4. `docs/governance/current-wave-2026-03-24.md`

## Work order

### Step 1

Execute:

- `docs/codex/COD-2026-03-24-001_维护团队后端基线任务.md`

### Step 2

Only after Step 1 is either fixed or explicitly blocked in writing, execute:

- `docs/codex/COD-2026-03-24-002_资产位置搜索后端基线任务.md`

### Step 3

When Lovable returns a plain-text frontend sync package, execute:

- `docs/codex/COD-2026-03-24-003_前端文本同步与本地验收任务.md`

## Hard constraints

- Do not modify frontend business code.
- Do not touch `lovable` repo for implementation.
- Do not reopen architecture.
- Do not change `/u/scan`, `/u/session`, `/u/history`, `/ops/orders`, `/ops/sessions` runtime semantics.
- Keep all new structure additive and auditable.

## Git rules

- Check `git status` first.
- This worktree may already be dirty; do not reset or revert unrelated changes.
- If the repo is dirty, isolate your edits and commit only task-related files.
- Do not include runtime logs, `.env`, or unrelated generated files in commits.
- Preferred commit split:
  - commit 1: maintenance-team backend baseline
  - commit 2: asset-location-search backend baseline

## Required result format

For each task, return:

1. task id
2. changed files
3. migration / contract summary
4. verification result
5. commit SHA
6. frontend impact
7. pending issue if blocked
