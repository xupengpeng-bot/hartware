# UAT Execution Standard

Status: active
Audience: PM, engineers, QA, AI agents
Purpose: define one stable UAT execution model so different AI agents do not diverge in quality, method, or target interpretation.

## Core rule

UAT is not one test command.

For this project, UAT is a layered acceptance process that must keep business truth, executable checks, and cleanup behavior aligned.

## UAT layers

Use these layers in order:

1. business and contract truth
2. backend build and automated verification
3. browser acceptance
4. protocol or device simulation when the scenario depends on runtime reporting
5. cleanup and closure

Do not skip directly to browser conclusions when the backend layer is still red.

## Source-of-truth split

### Human-facing business and UAT truth

Read from the business repository:

- `<BUSINESS_REPO_ROOT>\docs\uat\README.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\uat-plan-v1.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\frontend-backend-contract-checklist-v1.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\lovable-codex-sync.md`

### AI-facing execution truth

Read from the development-system workspace:

- `PROJECT-CONFIG-REGISTRY.md`
- `docs/codex/CURRENT.md`
- `docs/governance/file-only-command-protocol.md`
- `docs/governance/delivery-workflow.md`
- `docs/governance/uat-problem-solving-map.md`

## Standard UAT sequence

### 1. Preflight

- run `.\tools\preflight.ps1`
- confirm active task, task type, mode, and allowed working area

### 2. Backend baseline

At minimum:

- `npm run build`
- `npm run test:unit`

When acceptance requires it:

- `npm run test:e2e`

Do not treat browser-only observations as final truth if backend automated verification is already red.

### 3. Browser acceptance

Use local browser or Playwright-based execution when the task explicitly requires page-level validation.

Current frontend automation surface:

- Playwright config exists in the frontend repository
- local acceptance may still require headed browser verification for frozen business flows

### 4. Protocol or device simulation

If the scenario depends on runtime ingest, device packets, reconnect behavior, duplicate packets, order settlement, or alarm timing:

- use backend integration or e2e support first
- then use simulator or embedded handoff materials when the scenario goes beyond backend-only assertions

### 5. Cleanup and closure

Before closing UAT verification:

- run `npm run testdata:cleanup`

Closing a UAT task without cleanup is not allowed.

## UAT result expectations

UAT writeback should say clearly:

1. which layer passed
2. which layer was not executed
3. whether the result is backend-only, browser-only, or runtime/device dependent
4. whether test data cleanup has completed

## Hard anti-drift rule

AI must not:

- invent acceptance criteria from chat alone
- treat exploratory page clicking as sufficient UAT
- confuse mock-only success with real-mode closure
- treat device/runtime scenarios as complete without the proper simulation or ingest evidence
