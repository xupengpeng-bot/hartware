# Development-System Evolution Rule

Status: active
Audience: PM, engineers, AI agents
Purpose: let the development system improve during delivery without hijacking the business mainline.

## Core rule

The development system is expected to improve while delivery continues.

When a better method, template, script, onboarding improvement, or process guardrail is discovered during real work, it should be surfaced instead of forgotten.

## How to classify these items

Use this classification:

- task type:
  - `SYSTEM`
- shared task category:
  - `development_system`

This keeps development-system improvement work visible without pretending it is business feature delivery.

## How to report it

During any execution task, the assignee may report:

- `development-system improvement candidates`

These are observations or proposals, not automatically executable work.

## Hard gate

Do not silently replace the active business task with development-system optimization work.

If the improvement is not required to safely complete the current task:

- finish the active task
- record the candidate in the result writeback
- let PM decide whether to dispatch a separate `SYSTEM` task

If the improvement is required to unblock safe execution:

- keep it minimal
- keep it additive
- explain why it was necessary

## Good examples

- adding a preflight check script
- tightening the read order
- fixing recurring encoding issues in active entry docs
- adding a reusable task template
- splitting historical task sheets from active task sheets

## Bad examples

- expanding a business task into broad process refactoring without approval
- rewriting the whole development system because a local annoyance was noticed
- treating every personal preference as a system task
