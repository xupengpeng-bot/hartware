# Requirement Change Impact Standard

Status: active
Audience: PM, requirements engineer, software engineer, UAT engineer, AI agents
Purpose: make requirement changes carry explicit global impact markers so executors do not guess hidden side effects.

## Core rule

New requirements and changed requirements are not treated the same.

For a brand-new requirement, a simple scoped decomposition may be enough.

For a requirement change, the change owner must explicitly mark the likely impact surface before execution tasks are dispatched.

## Why this exists

Requirement changes are risky because the changed sentence may be small while the real impact is wide.

Typical hidden-impact areas:

- backend contracts
- frontend forms or selectors
- seed and baseline data
- UAT scenarios
- reporting or dashboards
- billing, runtime, audit, or device attribution
- simulator assumptions
- embedded or hardware handoff constraints

Executors must not discover these only after implementation has already started.

## Required impact sections for requirement changes

When a requirement document is modified in a way that changes behavior, scope, field meaning, or acceptance logic, include at least:

1. change summary
2. reason for change
3. global impact points
4. non-impacted areas
5. required follow-up roles
6. acceptance impact

## Minimum global impact points checklist

The requirement change owner should explicitly mark whether each area is:

- impacted
- not impacted
- unknown and needs follow-up

Required checklist:

1. business object model
2. backend API or response contract
3. frontend page, form, or selector behavior
4. database schema, seed, or baseline data
5. UAT scenario or acceptance criteria
6. browser automation or local verification flow
7. runtime, billing, audit, or session logic
8. protocol simulation or device-side assumptions
9. embedded or hardware handoff

## Dispatch rule

If a requirement change does not include an impact marker section, executable tasks should not assume the impact is local and isolated.

Instead:

- route back to requirements engineering
- request impact clarification

## Good example

Requirement change:

- Project region selector no longer uses the nationwide reference library and must select only from already-created business Regions

Expected impact markers:

- frontend selector logic: impacted
- backend contract: impacted or confirmed not impacted
- seeded demo data: impacted
- UAT regression path: impacted
- device runtime logic: not impacted

## Anti-drift rule

Do not hide wide impact behind small wording changes.

If the change alters the meaning of a field, object relation, workflow step, or acceptance rule, treat it as a requirement change with explicit impact markers.
