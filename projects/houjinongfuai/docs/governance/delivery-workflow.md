# Delivery Workflow

Status: active
Audience: PM, software engineer, Lovable, embedded engineer, hardware engineer, QA
Purpose: define one execution order for task dispatch, Git sync, local verification, and closure.

## Roles

- PM
  - owns priority, classification, frozen product rules, and task dispatch order
- software engineer
  - owns backend contract, migrations, tests, simulator skeletons, frontend task-package sync, and local verification support
- Lovable
  - owns frontend implementation in the frontend repo
- embedded engineer
  - owns firmware, protocol implementation, device logs, and simulator-side execution
- hardware engineer
  - owns board, module, connector, BOM, and physical interface constraints

## Repositories and working areas

- Backend workspace
  - current repository root
- Frontend workspace
  - sibling repo `../lovable`
- Embedded handoff folder
  - `./embeddedcomhis`
- Hardware handoff folder
  - `./hardwarecomhis`

Path rule:

- prefer repository-relative paths in active docs and task packages
- only freeze an absolute local path when a task explicitly depends on a machine-local location

## Global sequencing rule

- When a task depends on a new contract, protocol, schema, or hardware interface, upstream must land first.
- Frontend must not start against a contract that has not yet been frozen and pushed.
- Local acceptance happens only after all participating repos are pulled back to local.
- PM must fully update `CURRENT.md` before issuing any execute instruction in chat.
- Default command mode is file-only:
  - PM updates each team's `CURRENT.md`
  - PM updates queue boards such as `lovablecomhis/WAVE.md`
  - the user tells the named team only `execute`
  - the team writes back to `RESULT.md`

Dispatch completeness gate:

- PM may tell a team to execute only after `CURRENT.md` contains:
  - active task
  - work mode
  - execute-now instruction
- If chat and file disagree, treat that as PM dispatch failure and follow the file.

## Frontend throughput mode

Frontend work now uses:

1. batch sync
2. single active task
3. per-task local verification

### Batch sync rule

- PM should prepare a small batch of frontend tasks locally whenever the product rules are stable enough.
- software engineer should sync the batch into frontend GitHub `main` in one `SYNC` task.
- Lovable then executes only the active task already visible in Git `HEAD`.
- software engineer then runs one `VERIFY` task for that completed frontend item.

### When not to batch

Per-task sync is still allowed when:

- the task is an urgent hotfix
- the task corrects a just-frozen blocking product rule
- delaying sync would keep the queue misleading

## Frontend task flow

Use this flow for Lovable work.

1. PM classifies the issue as `A`, `B`, or `C`.
2. If backend patching is needed, software engineer lands the backend baseline first.
3. PM prepares or updates:
   - `../lovable/lovablecomhis/CURRENT.md`
   - `../lovable/lovablecomhis/WAVE.md`
   - the related task, context, and fixtures in the frontend repo
4. If the batch is not yet in frontend GitHub `main`, software engineer runs a `SYNC` task.
5. The user tells Lovable only `execute`.
6. Lovable reads `CURRENT.md`, checks `WAVE.md`, implements only the active scope, and writes back to `RESULT.md`.
7. Lovable platform auto-syncs the cloud result to GitHub.
8. software engineer runs a `VERIFY` task:
   - pull the frontend result back to local from Git
   - confirm `HEAD`, `origin/main`, and working-tree status before giving any negative verdict
   - run local acceptance against the pulled code
9. PM closes the task only after local verification and queue-state update.

## Software-engineer task flow

Use this flow for software engineer work.

PM should make the mode explicit in the task file and `CURRENT.md`:

- `BACKEND`
- `SYNC`
- `VERIFY`

If `CURRENT.md` still shows no active task, software engineer must refuse to proceed and report the missing dispatch entry instead of guessing.

### `SYNC`

- sync PM-created frontend task packages into frontend GitHub `main`
- do not mix frontend business-code edits into the sync commit

### `VERIFY`

- pull frontend GitHub `main` back to local from `../lovable`
- confirm `git rev-parse HEAD`
- confirm `git rev-parse origin/main`
- confirm `git status --short`
- run local build and acceptance
- decide whether the frontend task is locally closable

Hard gate:

- Any `not fixed`, `still broken`, or `not closable` verdict must be based on an up-to-date snapshot.
- If local `HEAD` is behind `origin/main`, the verifier must not reject the task from stale local code.
- If the working tree is dirty, the verifier must call out verification risk and avoid presenting the dirty workspace as remote-main truth.
- If `git fetch` or `git pull --ff-only origin main` fails, the verifier must not issue a final negative closure verdict against frontend implementation.
- In that case, the verifier may report only:
  - `verification risk`
  - `network-blocked`
  - `waiting_sync`
- A frontend implementation is considered actually landed only when a concrete Git commit containing the business-code files is visible from pulled `main` or confirmed `origin/main`.
- A textual completion report or `RESULT.md` entry without a visible implementation commit is not final truth.

### `BACKEND`

- update backend code, docs, tests, or simulator skeletons
- verify locally as far as the task requires
- update `docs/uat/lovable-codex-sync.md` if frontend expectations changed

## Embedded task flow

Use this flow for embedded engineer work.

1. PM creates or updates:
   - `embeddedcomhis/EMB-xxxx-*.md`
   - matching `context`
   - matching `fixtures`
2. PM updates `embeddedcomhis/CURRENT.md`.
3. The user tells embedded engineer only `execute`.
4. Task package must mirror any backend protocol truth the embedded team needs.
5. embedded engineer returns in `embeddedcomhis/RESULT.md`:
   - commit SHA or firmware package version
   - protocol log or test record
   - device sample identifiers
   - pending issues
6. PM updates protocol docs or sync docs if the interface changed.
7. Backend or simulator follow-up starts only after the embedded baseline is explicit.

## Hardware task flow

Use this flow for hardware engineer work.

1. PM creates or updates:
   - `hardwarecomhis/HW-xxxx-*.md`
   - matching `context`
   - matching `fixtures`
2. PM updates `hardwarecomhis/CURRENT.md`.
3. The user tells hardware engineer only `execute`.
4. Hardware task must define at least:
   - interface boundary
   - required module / connector / power assumptions
   - expected deliverable
5. hardware engineer returns in `hardwarecomhis/RESULT.md`:
   - revision number
   - BOM or drawing package reference
   - test result
   - pending issues
6. PM mirrors the result back into embedded/backend constraints if needed.

## Who goes first

### Case A: frontend-only issue

Order:

1. PM prepares the frontend batch and updates `CURRENT.md` plus `WAVE.md`
2. software engineer runs a `SYNC` task if the batch is not yet in GitHub `main`
3. the user tells Lovable to execute
4. Lovable executes in cloud and auto-syncs to GitHub
5. software engineer runs a `VERIFY` task through local Git pull
6. PM verifies locally

### Case B: backend gap plus frontend consumption

Order:

1. PM updates `docs/codex/CURRENT.md`
2. software engineer lands backend baseline
3. PM updates sync ledger
4. PM updates frontend `CURRENT.md` plus `WAVE.md`
5. software engineer runs a `SYNC` task if the frontend batch is not yet in GitHub `main`
6. the user tells Lovable to execute
7. Lovable executes in cloud and auto-syncs to GitHub
8. software engineer runs a `VERIFY` task through local Git pull
9. PM verifies joint integration locally

### Case C: hardware or embedded dependency exists

Order:

1. PM updates `hardwarecomhis/CURRENT.md` or `embeddedcomhis/CURRENT.md`
2. the user tells embedded engineer or hardware engineer to execute
3. hardware interface or embedded protocol baseline
4. backend adaptation
5. frontend adaptation
6. local end-to-end verification

## Closure rule

A task is not closed until:

- the task package is present in the correct handoff folder
- the implementation repo has a concrete commit or revision reference
- local verification has consumed the latest pulled code or artifact
- the queue board has been updated if the task is part of a visible batch
- any remaining ambiguity is written down as a pending issue

## Verification truth priority

When multiple truth sources disagree, use this order:

1. remote `origin/main`
2. local clean workspace after pull
3. execution-party textual report
4. PM local stale or dirty workspace

PM local stale workspace may be used as a suspicion signal, but not as the final basis for rejecting a completed task.
