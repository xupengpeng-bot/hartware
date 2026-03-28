# Role Init README

Status: active
Audience: PM, AI agents
Purpose: provide short role-specific initialization cards so new AI threads can start with the right task source, scope, and stop conditions.

## Use rule

When opening a new AI thread, PM should provide:

1. the role-init card for the intended role
2. the current task file or `CURRENT.md`
3. the relevant business requirement source when the task is not yet pure execution

## Available role-init cards

- `role-init/requirements_engineer.md`
- `role-init/feature_research_engineer.md`
- `role-init/software_engineer.md`
- `role-init/uat_engineer.md`
- `role-init/marketing_strategy_engineer.md`
- `role-init/development_system_engineer.md`
- `role-init/embedded_engineer.md`
- `role-init/hardware_engineer.md`

## Shared rule

- `CURRENT.md` is still the live dispatch entry for software-engineer execution.
- Role-init cards help a new thread understand how to read, where to start, and when to stop.
- If a role-init card conflicts with `CURRENT.md` or a PM-frozen task sheet, the explicit live task file wins.
