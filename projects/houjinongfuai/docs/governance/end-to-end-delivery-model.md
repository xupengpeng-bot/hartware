# End-To-End Delivery Model

Status: active
Audience: PM, delivery_orchestrator, all AI roles
Purpose: define how a fuzzy request can become delivered, verified work with minimal PM coordination and without role drift.

## Goal

The target operating mode is:

- PM gives a fuzzy demand
- the system routes it into the right stages
- each role receives only the context it needs
- delivery finishes with verification, cleanup, and structured writeback

## Core principle

One fuzzy request should not be pushed directly into code.

It should move through a controlled chain:

1. intake
2. clarification
3. decomposition
4. execution
5. verification
6. closure
7. optional development-system improvement capture

## Recommended primary controller

Use `delivery_orchestrator` as the top-level controller.

This role does not replace PM.

It reduces PM coordination work by:

- deciding the first owning role
- managing handoff order
- keeping active scope small
- checking that no required stage is skipped

## Standard stage flow

### Stage 1: intake

Owner:

- `delivery_orchestrator`

Input:

- fuzzy request from PM

Output:

- intake classification
- first recommended role
- initial route

Typical task types at this stage:

- `INTEREST`
- `SYSTEM`
- unready requirement-analysis work

### Stage 2: requirement clarification

Owner:

- `requirements_engineer`

Use when:

- the request is still fuzzy
- business meaning is unstable
- acceptance is not yet clear
- the request changes an existing requirement

Required output:

1. clarified goal
2. scope
3. non-goals
4. acceptance logic
5. global impact points when this is a requirement change
6. recommended execution split

### Stage 3: research when needed

Owner:

- `feature_research_engineer`

Use when:

- a solution path is still unclear
- options need to be compared first
- the right architecture slice is not yet obvious

Required output:

1. options
2. recommendation
3. risks
4. unknowns

### Stage 4: ready gate

Owner:

- PM plus `delivery_orchestrator`

Gate:

- `definition-of-ready.md`

The work should not become executable until:

- requirement source exists
- requirement understanding or requirement-change confirmation has happened when needed
- task type is explicit
- owner role is explicit
- allowed working area is explicit
- verification basis is explicit
- handoff target is explicit

### Stage 5: execution

Typical owner:

- `software_engineer`
- `frontend_engineer`
- `development_system_engineer`
- `embedded_engineer`
- `hardware_engineer`
- `marketing_strategy_engineer`

Rule:

- each executable task should stay narrow
- one role should receive one clear stage at a time
- the orchestrator should avoid passing the full project history into every thread

### Stage 6: verification

Owner:

- `uat_engineer` for acceptance
- or the owning execution role for build/unit-only checks

Required output:

1. verification result
2. evidence
3. failing layer when failed
4. cleanup status when UAT touched data
5. closure recommendation

### Stage 7: closure

Owner:

- PM or `delivery_orchestrator`

Required output:

1. closure status
2. next handoff target or closed state
3. unresolved issues
4. development-system improvement candidates when discovered

## Required role sequence by request shape

### Fuzzy product request

Recommended route:

1. `delivery_orchestrator`
2. `requirements_engineer`
3. `feature_research_engineer` if needed
4. `software_engineer`
5. `uat_engineer`
6. `delivery_orchestrator` or PM for closure

### Direct engineering bug or contract fix

Recommended route:

1. `delivery_orchestrator`
2. `software_engineer`
3. `uat_engineer` when acceptance is involved
4. `delivery_orchestrator` or PM for closure

### Marketing or business material request

Recommended route:

1. `delivery_orchestrator`
2. `marketing_strategy_engineer`
3. `requirements_engineer` only if product truth is unclear

### Workflow or tooling improvement

Recommended route:

1. `delivery_orchestrator`
2. `development_system_engineer`

## How this supports the PM expectation

The PM expectation is:

- say a fuzzy requirement once
- let the system take it through the rest of the chain

This is achievable if the system obeys three hard rules:

1. fuzzy demand enters through orchestration, not directly through engineering
2. requirement changes surface global impact before execution
3. PM confirms the high-risk misunderstanding points instead of micromanaging every task
4. verification and cleanup are never optional stages

## What still cannot be skipped

Even in the future, full automation should still stop briefly when:

- requirement change impact is too broad to infer safely
- the work touches device tools on a new machine and real tool paths are unknown
- verification evidence conflicts with business intent
- multiple roles would need to change frozen scope at once

In those cases, the system should ask the smallest possible blocking question, not restart the whole process.
