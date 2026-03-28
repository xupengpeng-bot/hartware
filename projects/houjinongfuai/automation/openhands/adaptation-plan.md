# OpenHands Adaptation Plan

Status: active
Audience: PM, delivery_orchestrator, software_engineer, development_system_engineer
Purpose: define how to use OpenHands as an execution platform without losing this project's task discipline, role routing, and verification quality.

## Decision

For this project, OpenHands should be treated as an execution runtime, not as the source of truth.

The system of record should remain:

1. business truth in `<BUSINESS_REPO_ROOT>`
2. workflow truth in `<PROJECT_DEV_ROOT>`
3. live task state in `CURRENT.md` now, and later dispatch DB when promoted to primary

OpenHands should execute within those boundaries.

## Why OpenHands is a good first platform

OpenHands is the most suitable first integration target when the goal is:

- execute code work repeatedly
- connect to Git repositories
- run local or remote engineering tasks
- later accept event-driven triggers

It is not expected to replace:

- requirements engineering
- requirement change impact analysis
- role routing
- UAT standards
- embedded and hardware tool-path confirmation

## What OpenHands should own

OpenHands should own only the execution-runtime layer:

- pull or sync repository state
- open a task-specific workspace
- read the approved entry chain
- perform bounded implementation or verification
- write back the structured result

## What OpenHands must not own

OpenHands must not become the decision-maker for:

- whether a fuzzy request is ready
- whether a requirement change is safe
- whether a historical task should be reactivated
- whether a device tool path may be assumed on a new machine
- whether scope expansion is allowed

Those decisions remain in:

- PM
- `delivery_orchestrator`
- `requirements_engineer`
- the frozen task files

## Layered architecture

### Layer 1: PM input

PM may start with:

- a fuzzy requirement
- a direct engineering bug
- a marketing request
- a workflow-improvement request

### Layer 2: orchestration

The top-level controller is:

- `delivery_orchestrator`

Its job is to:

- classify the request
- choose the first role
- split the delivery stages
- make sure verification and closure are present

### Layer 3: rule system

The rule system remains this development-system repository.

Its job is to define:

- role routing
- ready gate
- requirement change impact
- active entry chain
- result format
- UAT rules
- environment and path rules

### Layer 4: execution runtime

The execution runtime may be OpenHands.

Its job is to:

- read the approved task context
- run commands
- edit code
- verify results
- write structured output

### Layer 5: state

Current phase:

- file-first state using `CURRENT.md`

Future preferred phase:

- dispatch DB becomes the primary live lane/task state

### Layer 6: repositories and environments

The actual code and docs remain in:

- `<BUSINESS_REPO_ROOT>`
- `<FRONTEND_REPO_ROOT>`
- `<PROJECT_DEV_ROOT>`

## Minimum viable OpenHands integration

The first OpenHands integration should be intentionally narrow.

### Phase 1 scope

Support only:

- `software_engineer`
- task type `ENGINEERING`
- mode `BACKEND`

OpenHands should read only:

1. `docs/codex/START-HERE.md`
2. `<BUSINESS_REPO_ROOT>\AGENTS.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/codex/CURRENT.md`
5. `docs/codex/WORK-MODES.md`
6. `docs/codex/TASK-TYPES.md`
7. the active task file
8. `docs/codex/RESULT.md`

Phase 1 should not automate:

- fuzzy requirement decomposition
- marketing
- embedded and hardware work
- open-ended UAT browsing
- automatic scope expansion

## Recommended launch contract for OpenHands

Before OpenHands executes, the launch profile should enforce:

1. Git bootstrap completed
2. preflight passed
3. `CURRENT.md` has:
   - active task
   - task type
   - mode
4. task type is executable
5. role matches the requested execution role
6. allowed working area is present
7. verification basis is present

If any of these are missing, OpenHands should stop and report the blocking gate.

## Role rollout order

### First role to integrate

- `software_engineer`

### Second role to integrate

- `uat_engineer`

Only after `software_engineer` is stable should OpenHands be allowed to run acceptance-oriented verification.

### Later optional roles

- `development_system_engineer`
- `marketing_strategy_engineer`

These should come later because they require stronger template and result discipline.

### Do not automate early

- `embedded_engineer`
- `hardware_engineer`

These lanes still require explicit tool-path confirmation and real-environment judgment.

## Fuzzy-demand future state

The PM's desired future state is:

- PM says one fuzzy requirement
- the system handles the rest

With OpenHands, this should be implemented as:

1. PM gives fuzzy demand
2. `delivery_orchestrator` routes it
3. `requirements_engineer` sharpens it when needed
4. PM or orchestrator freezes ready scope
5. OpenHands executes only the approved executable stage
6. `uat_engineer` verifies
7. orchestrator closes and writes back follow-on recommendations

This means OpenHands is one part of the chain, not the whole chain.

## Hard stops OpenHands must obey

OpenHands must stop instead of guessing when:

1. `active task = none`
2. `task type = none`
3. mode is missing
4. requirement source is missing for a task that clearly depends on business truth
5. requirement change exists but global impact points are missing
6. the machine is new and the workspace is not bootstrapped
7. the lane touches embedded or hardware tooling and the real tool paths are not confirmed

## Result writeback contract

OpenHands output should continue to follow the fixed writeback shape:

1. task id
2. mode
3. status
4. changed files or synced files
5. verification result
6. commit SHA or `no git action`
7. pending issues
8. next handoff target

It should also add development-system improvement candidates when it discovers reusable process improvements.

## Success criteria for the first pilot

The first pilot should be considered successful only if:

1. OpenHands starts from the existing entry chain without extra PM explanation
2. it refuses to execute when the task is not ready
3. it stays within allowed working areas
4. it writes back the structured result
5. the verification step is not skipped
6. no historical task record is mistaken for the live task source

## Risks

### Risk 1: runtime becomes an implicit source of truth

Bad outcome:

- people trust OpenHands memory over task files

Control:

- always re-read the entry chain at launch
- file truth or dispatch DB truth remains primary

### Risk 2: fuzzy demand leaks directly into engineering

Bad outcome:

- OpenHands starts coding from vague intent

Control:

- always route fuzzy demand through `delivery_orchestrator`
- require DoR before executable launch

### Risk 3: too many roles are automated too early

Bad outcome:

- context drift
- noisy handoffs
- weak closure

Control:

- start with one role
- expand only after stable metrics

## Recommendation

Adopt OpenHands in this order:

1. Phase 1: `software_engineer` only
2. Phase 2: `software_engineer` plus `uat_engineer`
3. Phase 3: connect dispatch DB as stronger live state
4. Phase 4: consider workflow-triggered execution and broader role support

Do not replace the current development-system.

Use OpenHands to make the current system faster and more automatic.
