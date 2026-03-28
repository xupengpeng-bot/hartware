# UAT Problem Solving Map

Status: active
Audience: PM, engineers, QA, AI agents
Purpose: tell AI where to look first when UAT fails, so different agents converge on the same troubleshooting path.

## Core rule

Start from the failure type, not from intuition.

Different UAT failures belong to different truth sources and should not be debugged in one mixed loop.

## Problem-type map

### A. Environment or startup failure

Typical signals:

- service will not start
- port is occupied
- DB connection fails
- `.env` is missing or inconsistent

Read first:

1. `PROJECT-CONFIG-REGISTRY.md`
2. `docs/codex/NEW-MACHINE-ENV-BASELINE.md`
3. `.\tools\preflight.ps1`
4. startup scripts in the business repository

### B. Contract mismatch or page-data mismatch

Typical signals:

- page field is blank
- request shape is wrong
- selector options are wrong
- response shell differs from expectation

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\docs\uat\frontend-backend-contract-checklist-v1.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\uat\lovable-codex-sync.md`
3. relevant business requirement docs
4. backend tests and response model code

### C. Browser behavior or interaction failure

Typical signals:

- click path does not work
- dialog cannot close
- validation is wrong
- page flow only fails in UI

Read first:

1. frontend task package and fixtures
2. frontend Playwright config and fixtures
3. `D:\20251211\zhinengti\houjinongfuai\docs\uat\uat-plan-v1.md`
4. local browser acceptance evidence

### D. Seed, data, or reset failure

Typical signals:

- expected options are missing
- region library is incomplete
- old UAT data pollutes current verification
- local verification differs after reseed

Read first:

1. `PROJECT-CONFIG-REGISTRY.md`
2. backend `package.json` scripts
3. backend seed and cleanup scripts
4. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`

### E. Runtime or device-report scenario failure

Typical signals:

- duplicate packet handling is wrong
- out-of-order packets break state
- device reconnect behavior is wrong
- billing or session closure differs after runtime ingest

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\docs\uat\uat-plan-v1.md`
2. backend integration and e2e tests
3. backend `test/support/seed-scenarios.ts`
4. embedded handoff package if runtime protocol truth is involved

### F. Firmware or hardware dependent failure

Typical signals:

- backend is fine, but the board does not report correctly
- serial behavior differs from simulator assumptions
- flash, debug, or power/interface behavior blocks closure

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\embeddedcomhis\CURRENT.md`
2. `D:\20251211\zhinengti\houjinongfuai\embeddedcomhis\README.md`
3. `D:\20251211\zhinengti\houjinongfuai\hardwarecomhis\CURRENT.md`
4. `D:\20251211\zhinengti\houjinongfuai\hardwarecomhis\README.md`

If embedded or hardware is paused, do not pretend the runtime/device issue is closed.

## Simulation map

### Browser simulation

Current place to look:

- `D:\20251211\zhinengti\lovable\playwright.config.ts`
- `D:\20251211\zhinengti\lovable\playwright-fixture.ts`

### Backend or protocol simulation

Current place to look:

- backend integration tests
- backend e2e tests
- backend test support and seed scenarios

### Device-side simulation

Current place to look:

- `embeddedcomhis/fixtures/*`
- `hardwarecomhis/fixtures/*`

## Escalation rule

If a failure crosses layers:

1. identify the first failing layer
2. stop there
3. report the next dependent layer as blocked, not failed by assumption

Example:

- if backend contract is red, browser UAT is blocked
- if firmware is paused, runtime real-device closure is blocked

## Consistency rule for all AI agents

All AI agents should follow the same order:

1. classify the failure type
2. read the mapped truth sources
3. state which layer is actually failing
4. avoid jumping to a different layer without evidence
