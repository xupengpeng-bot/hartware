# Role Lane Catalog

Status: active
Audience: PM, engineers, AI agents
Purpose: define stable role names so AI execution stays aligned even when context windows differ.

## Core rule

Every formal task should identify the primary delivery role.

Task type explains the nature of the work.
Role lane explains who should read it, execute it, or decompose it.

## Standard role lanes

### `requirements_engineer`

Focus:

- receive fuzzy product intent
- clarify scope and business language
- update requirement docs
- decompose approved requirements into executable task candidates

Default output:

- requirement updates
- scope split
- acceptance criteria draft
- open questions list

### `feature_research_engineer`

Focus:

- exploration
- feasibility
- product or technical comparison
- new capability pre-research

Default output:

- analysis note
- option comparison
- recommendation
- risks and unknowns

### `software_engineer`

Focus:

- backend implementation
- migrations
- tests
- sync and verify support

Default output:

- code
- tests
- verification result

### `frontend_engineer`

Focus:

- frontend implementation
- UI behavior
- page-level acceptance support

Default output:

- frontend code
- page-level evidence

### `uat_engineer`

Focus:

- acceptance execution
- scenario running
- browser evidence
- cleanup and closure evidence

Default output:

- UAT evidence
- failing layer classification
- cleanup status
- closure recommendation

### `embedded_engineer`

Focus:

- firmware
- protocol handling
- simulator-side or device-side execution

### `hardware_engineer`

Focus:

- board and interface constraints
- power and connector assumptions

### `development_system_engineer`

Focus:

- workflow
- governance
- onboarding
- automation
- preflight
- standards and tooling improvements

## Role rule

- If a task is fuzzy and not yet executable, route it first to `requirements_engineer` or `feature_research_engineer`, not directly to `software_engineer`.
- If a task is about acceptance quality, closure, or execution evidence, route it to `uat_engineer`.
- If a task changes workflow or the task system itself, route it to `development_system_engineer`.

## Anti-drift rule

Do not let one role silently absorb another role's job just because the same human or AI is currently acting in multiple roles.

Even in single-person operation, the role lens must stay explicit.
