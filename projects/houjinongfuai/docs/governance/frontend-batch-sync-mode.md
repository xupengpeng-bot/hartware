# Frontend Batch Sync Mode

Status: active
Audience: PM, 软件工程师, Lovable
Purpose: improve throughput by batching frontend task-package synchronization instead of syncing every new frontend task one by one.

## Core change

Frontend handoff files should be synced in batches, not per-task by default.
The target cadence is 3 to 5 times the previous throughput by reducing sync churn and reducing user-facing handoff turns.

## Batch rule

1. PM prepares a medium batch of frontend tasks locally.
   - default target: 2 to 4 tasks or one coherent feature slice
2. PM marks each task in one of these states:
   - `draft_local_only`
   - `synced_ready`
   - `active`
   - `waiting_verify`
   - `closed`
   - `superseded`
3. 软件工程师 performs one `SYNC` task for the whole batch.
4. Lovable executes only tasks already marked `synced_ready` or `active` in the frontend repo.
5. 软件工程师 performs one `VERIFY` run per completed frontend task.
6. PM should avoid interrupting an active batch unless a frozen product rule is truly wrong.

## User-facing interaction rule

To reduce back-and-forth, PM should surface the user only at batch gates:

1. batch freeze confirmed
2. batch sync completed
3. active item locally verified

Do not re-open scope during normal execution unless the product rule is materially wrong.

## When not to batch

Immediate single-task sync is still allowed when:

- a task is an urgent hotfix
- a task corrects a just-frozen product rule and blocks the current active item
- the batch would otherwise delay an already validated upstream baseline

## Required visible queue

The frontend repo should keep a readable queue board:

- `..\lovable\lovablecomhis\WAVE.md`

`CURRENT.md` should stay lightweight and point to the active item.
`WAVE.md` should show the batch and the state of adjacent tasks.
