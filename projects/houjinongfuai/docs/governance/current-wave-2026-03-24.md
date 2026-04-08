# Current Wave 2026-03-24

Status: active
Audience: PM, Cursor, Lovable, embedded engineer, hardware engineer
Purpose: define the current coordinated execution wave after documentation cleanup and protocol stabilization.

## Wave target

Advance the larger-granularity development wave:

1. Freeze the backend Phase 1 skeleton for blocks, metering points, network models, permission grain, and solver contracts
2. Sync the prepared frontend package for project configuration and cockpit integration
3. Complete the first frontend shell wave and first real-contract wiring wave
4. Close the backend/frontend real-mode contract mismatch found during `LVB-4021` verification
5. Start the second frontend interaction-closure wave for blocks and metering points

## Execution order

1. PM
   - freeze large-granularity packages
   - keep one active execution owner at a time on the critical path
2. Lovable
   - completed `LVB-4020`
   - completed `LVB-4021` first real-contract wiring wave
3. Cursor
   - active on syncing `LVB-4022` handoff into frontend Git `main`

## Current readiness

### Ready now

- `COD-2026-03-26-013`
  - completed
  - backend skeleton freeze for blocks, metering points, network models, data-scope grain, and solver contracts
- `COD-2026-03-26-014`
  - completed
  - synced `LVB-4020` package into frontend Git `main`
- `LVB-4020`
  - completed
  - locally verified and closable for the first-batch scope
- `COD-2026-03-26-015`
  - completed
  - real backend contract wave for blocks and metering points
- `COD-2026-03-26-016`
  - completed
  - synced `LVB-4021` package into frontend Git `main`
- `LVB-4021`
  - completed
  - local re-verify passed after backend compatibility alignment
- `COD-2026-03-26-019`
  - active
  - sync `LVB-4022` handoff into frontend Git `main`

### Explicitly paused

- `EMB-0001`
  - pause until device integration wave
- `HW-0001`
  - pause until device integration wave

## PM enforcement rule

- Execution is driven by each team's `CURRENT.md` file.
- The user only needs to tell the named team to execute.
- Current critical path owner is Cursor.
- Next frontend follow-up is `LVB-4022` after `COD-2026-03-26-019` sync completes.
