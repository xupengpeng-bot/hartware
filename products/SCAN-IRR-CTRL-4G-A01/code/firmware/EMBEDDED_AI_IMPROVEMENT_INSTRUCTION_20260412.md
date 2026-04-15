# Embedded AI Improvement Instruction - Real Device Test Round - 2026-04-12

Generated from the on-thread field test instruction set on `2026-04-12`.

## Device

- IMEI: `861295087573980`

## Round Summary

- automatic scenarios: `13`
- failed scenarios: `13`
- initial `sent` residue: `4`
- ending `sent` residue: `14`

## Highest Priority Fix Goals

1. Every `qcs / qwf / qem / spu / tpu / pas / res` reply must carry complete correlation information so platform commands do not remain in `sent`.
2. Query paths must return `QS` reliably while the device is online.
3. Under stress, the device must keep draining the command queue instead of accumulating `sent` residue.
4. Invalid-state `pause / resume / stop(no session)` must always return an explicit `NK`.
5. Meter queries must return explainable cumulative values that remain monotonic within the same meter epoch.

## Scenario List

### CONN-001

- issue: `heartbeat should keep device online and connected`
- failure: `unresolved_commands:QUERY_COMMON_STATUS`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### CONN-005

- issue: `every AK or NK or QS must carry command correlation`
- failure: `unresolved_commands:QUERY_WORKFLOW_STATE,stop_pump`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### STRESS-008

- issue: `query and control cross-fire should stay correlated`
- failure: `queue_sent_residue:19`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths
  - fix burst stability and queue drain behavior

### STRESS-001

- issue: `query storm should not explode sent backlog`
- failure: `queue_sent_residue:20`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths
  - fix burst stability and queue drain behavior

### STRESS-002

- issue: `mixed command storm should expose illegal state handling clearly`
- failure: `queue_sent_residue:22`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths
  - fix burst stability and queue drain behavior

### METER-007

- issue: `same epoch counters must remain monotonic`
- failure:
  - `queue_sent_residue:3`
  - `metrics_missing:rt`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths
  - fix cumulative counter semantics and meter epoch reporting

### PAUSE-002

- issue: `pause in invalid state should return NK and rollback`
- failure:
  - `unresolved_commands:pause_session`
  - `queue_sent_residue:5`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### PAUSE-004

- issue: `resume in invalid state should stay paused`
- failure:
  - `unresolved_commands:resume_session`
  - `queue_sent_residue:4`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### CMD-007

- issue: `stop when no session is running should return explicit NK`
- failure:
  - `unresolved_commands:stop_pump`
  - `queue_sent_residue:15`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### QUERY-001

- issue: `qcs should return common status result`
- failure:
  - `unresolved_commands:QUERY_COMMON_STATUS`
  - `expected_query_success:QUERY-001:sent`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### QUERY-002

- issue: `qwf should return workflow state result`
- failure:
  - `unresolved_commands:QUERY_WORKFLOW_STATE`
  - `expected_query_success:QUERY-002:sent`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### QUERY-003

- issue: `qem should return electric meter result`
- failure:
  - `unresolved_commands:QUERY_ELECTRIC_METER`
  - `expected_query_success:QUERY-003:sent`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

### QUERY-005

- issue: `query storm should stay correlated under concurrency`
- failure:
  - `unresolved_commands:QUERY_COMMON_STATUS,QUERY_WORKFLOW_STATE,QUERY_ELECTRIC_METER`
  - `stress_residue_sent:14`
- focus:
  - fix protocol correlation gaps
  - fix state machine consistency
  - prevent silent timeout paths

## Manual Validation Reminder

These scenarios require real-device validation evidence, not only code claims:

- pump power loss
- reconnect after network outage
- emergency stop
- fault stop
- same-card second swipe
- swipe-event resend after power loss
- `meter_epoch` changes
- `counter_reset`
