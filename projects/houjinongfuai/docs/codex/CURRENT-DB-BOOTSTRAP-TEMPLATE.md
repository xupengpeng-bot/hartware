# Current DB Bootstrap Template

Status: template
Audience: PM, software engineer, Cursor, Codex
Purpose: minimal local bootstrap file when dispatch DB becomes the live task source.

## Minimal shape

```text
status: active
team: software_engineer
source of truth: dispatch_db
fallback: stop and report dispatch db unavailable

read order:
1. fetch lane bootstrap from dispatch DB
2. fetch active task state from dispatch DB
3. if task type is not explicit, stop
4. if active task is none, report no active task and stop

local reminder:
- files are onboarding and fallback only
- chat is not the task source of truth
- do not guess task type
```

## Required DB fields

The matching dispatch DB bootstrap should expose at minimum:

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

Recommended additional fields:

- `title`
- `purpose`
- `source_file`
- `summary_json`
- `execute_now_md`

## Recommended bootstrap reads

Preferred backend API:

- `GET /api/v1/dispatch/team/:team/bootstrap`

Optional direct MySQL helper:

- `python backend/scripts/dispatch_bootstrap_fetch.py --team software_engineer`

## Safety rule

If the DB bootstrap does not distinguish task type, do not enable DB-primary execution yet.
