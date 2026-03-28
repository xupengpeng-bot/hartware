# Task Taxonomy Matrix

Status: active
Audience: PM, engineers, AI agents
Purpose: map project task types to shared task categories so dispatch uses one consistent language.

## Why this exists

There are two useful classification layers:

1. project task type
   - describes the nature of the current item
2. shared task category
   - describes how the item should be managed in the broader delivery system

They are related, but they are not identical.

## Project task types

- `INTEREST`
  - exploration, feasibility, comparison, question answering
- `ENGINEERING`
  - executable development, sync, verify, migration, schema, code, tests
- `LANGUAGE`
  - translation, naming, wording, copy cleanup
- `SYSTEM`
  - process, workflow, governance, tooling-system change

## Shared task categories

- `business_delivery`
  - directly advances business capability
- `acceptance_collaboration`
  - sync, verify, build, test, handoff, acceptance confirmation
- `development_system`
  - improves the development process, task system, automation, or tooling

## Recommended mapping

| Task type | Common category | Notes |
|------|---------|---------|
| `INTEREST` | none by default | usually analysis only; becomes a formal category only after PM freezes follow-up work |
| `ENGINEERING` | `business_delivery` or `acceptance_collaboration` | use delivery for real feature/fix work; use acceptance for sync/verify/build/handoff work |
| `LANGUAGE` | `business_delivery` or `development_system` | business wording belongs to delivery; prompt/process wording belongs to development-system |
| `SYSTEM` | `development_system` | do not silently reclassify as business delivery |

## Dispatch rule

When dispatching a typed task, PM should set both:

1. task type
2. shared task category

This removes ambiguity such as:

- an `ENGINEERING` item that is really only verification work
- a `SYSTEM` item that should not displace the business mainline
- a `LANGUAGE` item that belongs to tooling rather than product wording

## Priority rule

If task type and task category appear to disagree:

1. re-check the requirement source
2. re-check the intended output
3. fix the task before execution

Do not let the assignee guess the true classification.
