# Dispatch DB Pilot

Status: pilot
Audience: PM, Cursor, Lovable
Purpose: trial a database-backed task-state mode while keeping Git as code truth and local build as verification truth.

## 1. Pilot goal

Reduce cross-repo task-state sync friction by storing live dispatch state and copied handoff materials in a separate dispatch database.

This pilot does **not** replace:

- Git as implementation truth
- pulled `main` as verification truth
- local build / local acceptance as closure truth

## 2. Database

- engine: MySQL
- host: configured through local env
- database: configured through local env
- current connection proof: direct external host is reachable; proxy host was refused during initial probe

## 3. Tables

- `dispatch_team_current`
  - one live row per team
  - stores active task, mode, and brief execute-now text
- `dispatch_task`
  - one task record per migrated active task
  - stores task metadata plus raw markdown payload
  - should now also distinguish `task_type` so discussion work is not mistaken for engineering work
- `dispatch_artifact`
  - stores copied markdown artifacts such as:
    - `docs/codex/*.md`
    - `lovablecomhis/*.md`
    - `lovablecomhis/context/*.md`
    - `lovablecomhis/fixtures/*/README.md`

## 4. Pilot truth rule

Use this priority during the pilot:

1. Git implementation commit and pulled `main`
2. local build / local verify
3. dispatch database state
4. file copies

The dispatch DB improves task-state consistency, but it does not override Git or verification truth.

## 5. Current pilot scope

The pilot has already migrated:

- current backend dispatch docs
- current frontend handoff docs
- active frontend batch package for `LVB-4038`

The pilot currently stores copied markdown and live team state; it does not yet fully replace file-based execution.

## 5.1 Minimal executable target

The next practical target is not a full workflow platform.

It is a minimal DB-executable bootstrap:

1. local `CURRENT.md` becomes a tiny bootstrap or fallback note
2. dispatch DB stores the active lane row
3. dispatch DB stores the active task with explicit `task_type`
4. agent reads one bootstrap response and then executes only that typed task

This prevents a new machine agent from confusing:

- curiosity discussion
- system-process discussion
- language work
- real engineering delivery

Template for the minimal local bootstrap file:

- `docs/codex/CURRENT-DB-BOOTSTRAP-TEMPLATE.md`

## 5.2 Minimal required fields

At minimum, the dispatch DB should provide:

- lane row
  - `team`
  - `status`
  - `active_task_id`
  - `work_mode`
- task row
  - `task_id`
  - `task_type`
  - `mode`
  - `status`

Recommended:

- `title`
- `purpose`
- `source_file`
- `summary_json`
- `execute_now_md`

## 6. Probe

Local probe script:

- `backend/scripts/dispatch_db_probe.py`

Required env vars:

- `DISPATCH_DB_ENABLED`
- `DISPATCH_DB_HOST`
- `DISPATCH_DB_PORT`
- `DISPATCH_DB_NAME`
- `DISPATCH_DB_USER`
- `DISPATCH_DB_PASSWORD`

Optional local direct-read helper:

- `backend/scripts/dispatch_bootstrap_fetch.py --team software_engineer`

## 7. When to roll forward

Move further into DB-backed mode only if both are true:

1. Cursor can reliably read/write the dispatch DB
2. Lovable can also consume the DB-backed state without increasing task ambiguity

If Lovable cannot reliably consume DB state, keep files as the live execution entry and treat the DB as a mirrored task-state cache.
