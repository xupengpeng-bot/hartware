# Task Types

Status: active
Audience: PM, software engineer, Cursor, Codex, Lovable
Purpose: prevent execution confusion by separating discussion, delivery, language work, and system-governance work into explicit task types.

## Core rule

Every active task must declare one `task type`.

If `CURRENT.md` or the active task file does not make the task type explicit, treat the task as not dispatched for execution.

Files are the source of truth. Chat is only supplemental context.

## Allowed task types

### `INTEREST`

Meaning:

- exploratory question
- curiosity-driven research
- comparison, tradeoff, or feasibility discussion
- problem framing before a real delivery task exists

Default execution rule:

- do not change code
- do not change production-facing contracts
- do not open cross-repo implementation work
- return analysis, summary, or recommendations only

### `ENGINEERING`

Meaning:

- real development work
- migration, DDL, seed, contract, test, backend stabilization
- bug fix, refactor, compatibility patch, or delivery handoff

Default execution rule:

- executable only when `CURRENT.md` also names:
  - active task id
  - task type = `ENGINEERING`
  - work mode such as `BACKEND`, `SYNC`, or `VERIFY`
  - execute-now instruction

### `LANGUAGE`

Meaning:

- translation
- wording cleanup
- bilingual docs
- naming or copy editing

Default execution rule:

- change only the requested text scope
- do not infer engineering implementation work from language tasks

### `SYSTEM`

Meaning:

- process design
- workflow or governance adjustment
- onboarding, protocol, or tooling-system changes
- task system or permission-system design

Default execution rule:

- do not treat system discussion as product or backend delivery automatically
- only change governance, onboarding, protocol, or tooling files named by the active task
- do not reopen product architecture or business scope unless PM explicitly dispatches it

## Dispatch safety rule

The following are not executable engineering tasks by default:

1. architecture discussion
2. governance discussion
3. permission-model discussion
4. cloud-vs-local process discussion
5. onboarding improvement discussion
6. curiosity questions or "what do you think" prompts

These may become executable only after PM freezes them into files as a typed active task.

## Required dispatch fields

`CURRENT.md` and the active task file should together make these fields explicit:

1. task id
2. task type
3. mode
4. status
5. allowed working area
6. execute-now instruction

If any of `task id`, `task type`, or `mode` is missing for an executable task, stop and report dispatch ambiguity.

## Current practical rule for this repository

At the current Phase 1 stage:

- backend migration, DDL, seed, test, contract, and stabilization work should be typed as `ENGINEERING`
- frontend handoff sync and local acceptance should still be typed as `ENGINEERING`
- docs-only process cleanup should normally be typed as `SYSTEM`
- research or "which approach is better" questions should normally be typed as `INTEREST`
- translation or naming refinement should normally be typed as `LANGUAGE`

## Anti-confusion examples

Example A:

- user asks whether cloud development is suitable
- type = `INTEREST`
- correct result = analysis only

Example B:

- user asks to design task authorization architecture
- type = `SYSTEM`
- correct result = governance doc updates only

Example C:

- PM activates migration baseline task in `CURRENT.md`
- type = `ENGINEERING`
- mode = `BACKEND`
- correct result = backend code and test work may proceed

Example D:

- user asks to polish bilingual wording in docs
- type = `LANGUAGE`
- correct result = text edits only
