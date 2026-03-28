# Role-Based Delivery Model

Status: active
Audience: PM, engineers, AI agents
Purpose: keep task routing stable by making the project's working roles explicit.

## Core rule

This project is allowed to use different role lenses even when one human or one AI performs multiple roles.

Role clarity reduces context drift.

## Role routing for this project

### `requirements_engineer`

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\requirements\README.md`
3. the business system-docs README under `D:\20251211\zhinengti\houjinongfuai\docs`
4. `D:\20251211\zhinengti\houjinongfuai\docs\uat\README.md`
5. execution history in active task sheets and `RESULT.md`

### `feature_research_engineer`

Read first:

1. business requirement docs relevant to the topic
2. `docs/codex/TASK-TYPES.md`
3. `docs/governance/requirements-engineering-standard.md`

Default task type:

- `INTEREST`

### `marketing_strategy_engineer`

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\requirements\README.md`
3. the business system-docs README under `D:\20251211\zhinengti\houjinongfuai\docs`
4. `D:\20251211\zhinengti\development-system\projects\houjinongfuai\PROJECT-MARKETING-BRIEF.md`
5. `docs/governance/marketing-strategy-standard.md`

Default task types:

- `INTEREST`
- `LANGUAGE`

### `software_engineer`

Read first:

1. `CURRENT.md`
2. `PROJECT-CONFIG-REGISTRY.md`
3. delivery and UAT governance docs

Default task type:

- `ENGINEERING`

### `frontend_engineer`

Read first:

1. frontend task package
2. `D:\20251211\zhinengti\houjinongfuai\docs\uat\lovable-codex-sync.md`
3. contract checklist

### `uat_engineer`

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\docs\uat\README.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\uat\uat-plan-v1.md`
3. `docs/governance/uat-execution-standard.md`
4. `docs/governance/uat-problem-solving-map.md`
5. `PROJECT-CONFIG-REGISTRY.md`

Default task category:

- `acceptance_collaboration`

### `embedded_engineer`

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\embeddedcomhis\CURRENT.md`
2. `D:\20251211\zhinengti\houjinongfuai\embeddedcomhis\README.md`

### `hardware_engineer`

Read first:

1. `D:\20251211\zhinengti\houjinongfuai\hardwarecomhis\CURRENT.md`
2. `D:\20251211\zhinengti\houjinongfuai\hardwarecomhis\README.md`

### `development_system_engineer`

Read first:

1. shared global rules
2. project governance docs
3. `ROOT-WORKSPACE-MAP.md`
4. `PROJECT-CONFIG-REGISTRY.md`

Default task type:

- `SYSTEM`

## Dispatch rule

Every executable task should include:

1. `owner role`
2. `task type`
3. `task category`
4. `mode` when relevant

This gives different AI agents a consistent perspective even if they were not part of the earlier conversation.
