# Houjinongfuai Project Config Registry

Status: active
Audience: PM, engineers, AI agents
Purpose: provide one stable lookup point for project configuration, startup surfaces, and troubleshooting order.

## Workspace anchors

Use workspace-relative anchors on each machine:

- `WORKSPACE_ROOT`
- `BUSINESS_REPO_ROOT = <WORKSPACE_ROOT>\houjinongfuai`
- `DEVSYSTEM_REPO_ROOT = <WORKSPACE_ROOT>\development-system`
- `PROJECT_DEV_ROOT = <DEVSYSTEM_REPO_ROOT>\projects\houjinongfuai`
- `FRONTEND_REPO_ROOT = <WORKSPACE_ROOT>\lovable`

Do not assume the same drive letter or parent path as another machine.

## Active repositories

- backend business repository:
  - `<BUSINESS_REPO_ROOT>`
- frontend repository:
  - `<FRONTEND_REPO_ROOT>`
- development-system repository:
  - `<DEVSYSTEM_REPO_ROOT>`

## Env files

### Backend

- template:
  - `<BUSINESS_REPO_ROOT>\backend\.env.example`
- local file:
  - `<BUSINESS_REPO_ROOT>\backend\.env`

Primary key groups:

- runtime:
  - `NODE_ENV`
  - `PORT`
- PostgreSQL:
  - `POSTGRES_HOST`
  - `POSTGRES_PORT`
  - `POSTGRES_DB`
  - `POSTGRES_USER`
  - `POSTGRES_PASSWORD`
  - `DATABASE_URL`
- optional dispatch mirror:
  - `DISPATCH_DB_ENABLED`
  - `DISPATCH_DB_HOST`
  - `DISPATCH_DB_PORT`
  - `DISPATCH_DB_NAME`
  - `DISPATCH_DB_USER`
  - `DISPATCH_DB_PASSWORD`
  - `DISPATCH_DB_WRITE_ENABLED`
  - `DISPATCH_WRITE_KEY`

### Frontend

- template:
  - `<FRONTEND_REPO_ROOT>\.env.example`
- local file:
  - `<FRONTEND_REPO_ROOT>\.env`

Primary key groups:

- frontend API mode:
  - `VITE_API_MODE`
- backend API target:
  - `VITE_API_BASE_URL`

## Runtime ports

- backend default:
  - `3000`
- frontend Vite default:
  - `5173`
- PostgreSQL default:
  - `5432`
- optional dispatch MySQL default:
  - `3306`

## Database defaults

### PostgreSQL

Expected default local development target from the backend template:

- host:
  - `127.0.0.1`
- port:
  - `5432`
- database:
  - `houji_p1`

### Dispatch MySQL mirror

This is optional and disabled by default.

- enabled by:
  - `DISPATCH_DB_ENABLED=true`
- purpose:
  - dispatch read or write mirror for bootstrap and workflow support

## Startup scripts

## Git bootstrap remotes

- business repository:
  - `https://github.com/xupengpeng-bot/houjinongfuai.git`
- development-system repository:
  - `https://github.com/xupengpeng-bot/development-system.git`
- frontend repository:
  - PM-provided remote when frontend work is required

## DB bootstrap

When dispatch DB is active, use DB bootstrap to obtain the live lane and active task after Git bootstrap.

Preferred direct helper:

- `python <BUSINESS_REPO_ROOT>\backend\scripts\dispatch_bootstrap_fetch.py --team software_engineer`

The DB bootstrap should be treated as live task-state truth for:

- team lane
- active task id
- task type
- mode
- status
- optional execute-now summary

### Backend startup

- script:
  - `<BUSINESS_REPO_ROOT>\start-backend.ps1`
- behavior:
  - creates backend `.env` from template if needed
  - installs dependencies if `node_modules` is missing
  - runs `npm run db:up`
  - runs migration or reset flow
  - optionally applies seed profile
  - starts NestJS dev server

### Frontend startup

- script:
  - `<BUSINESS_REPO_ROOT>\start-frontend.ps1`
- behavior:
  - resolves frontend repo as sibling `..\lovable` unless overridden
  - creates frontend `.env` from template if needed
  - installs dependencies if `node_modules` is missing
  - starts Vite on `127.0.0.1:5173`

## Verification entry commands

### Development-system

- preflight:
  - `<PROJECT_DEV_ROOT>\tools\preflight.ps1`

### Backend

- install:
  - `cd <BUSINESS_REPO_ROOT>\backend`
  - `npm install`
- DB and migration:
  - `npm run db:up`
  - `npm run db:migrate`
- verification:
  - `npm run build`
  - `npm run test:unit`
  - `npm run test:e2e`

### Frontend

- install:
  - `cd <FRONTEND_REPO_ROOT>`
  - `npm install`
- dev:
  - `npm run dev`

## UAT and simulation entry points

### Human-facing UAT truth

- `<BUSINESS_REPO_ROOT>\docs\uat\README.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\uat-plan-v1.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\frontend-backend-contract-checklist-v1.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\lovable-codex-sync.md`

### Backend UAT support

- `npm run test:e2e`
- `npm run testdata:cleanup`
- backend integration tests in:
  - `<BUSINESS_REPO_ROOT>\backend\test\integration`
- backend e2e tests in:
  - `<BUSINESS_REPO_ROOT>\backend\test\e2e`

### Frontend browser acceptance support

- `<FRONTEND_REPO_ROOT>\playwright.config.ts`
- `<FRONTEND_REPO_ROOT>\playwright-fixture.ts`

### Device and protocol-side handoff support

- `<BUSINESS_REPO_ROOT>\embeddedcomhis`
- `<BUSINESS_REPO_ROOT>\hardwarecomhis`

## Tooling dependencies

Expected commonly available tools:

- `git`
- `node`
- `npm`
- `python`
- `docker`
- `arm-none-eabi-gcc`
- `openocd`

## New-environment tool-path confirmation

For a new machine or new bench environment, embedded and hardware work must confirm the real executable paths with PM/user before continuing.

Minimum confirmation set:

- `arm-none-eabi-gcc`
- `openocd`
- `st-flash` or `STM32_Programmer_CLI`
- `stm32flash` when serial flashing is needed
- serial-terminal tool path when UART observation is part of the task

Do not assume the previous machine's path layout still applies.

Windows-specific guards:

- system long paths enabled
- Git `core.longpaths=true`
- Chinese documentation kept readable in the Windows-first workflow

## Secret rule

- do not store real secrets in this file
- do not commit local `.env`
- use this file to record key names, ownership, defaults, and lookup paths only

## Troubleshooting order

When startup or verification fails, use this order:

1. confirm Git bootstrap completed and the repositories are in the intended workspace root
2. run `.\tools\preflight.ps1`
3. confirm you are in the correct workspace roots
4. confirm the expected `.env` files exist
5. confirm ports and DB targets from `.env.example`
6. confirm Docker and DB container availability
7. confirm Git workspace cleanliness and branch state
8. when dispatch DB is active, confirm DB bootstrap returns the intended lane/task state
9. then inspect the specific startup script or failing command

## Ownership

- business runtime config truth:
  - business repository files
- development-system config lookup truth:
  - this registry
- machine-local secret truth:
  - local `.env` or user environment variables
