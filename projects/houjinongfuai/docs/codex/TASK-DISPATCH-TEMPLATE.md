# Task Dispatch Template

Status: active-template
Audience: PM, requirements engineer, development-system engineer
Purpose: create new executable task sheets without copying historical task records.

## Required fields

1. `task id`
2. `requirement source`
3. `task type`
4. `task category`
5. `owner role`
6. `mode`
7. `status`
8. `allowed working area`
9. `global impact points` when derived from a requirement change
10. `execute now`
11. `verification`
12. `next handoff target`

## Template

```md
# TASK-ID Title

Status: active
Owner role: `software_engineer`
Task type: `ENGINEERING`
Task category: `business_delivery`
Mode: `BACKEND`

## Requirement source

- `absolute/path/to/business-requirement.md`

## Allowed working area

- `repo/or/path`

## Global impact points

- `not applicable` or explicit impact list

## Execute now

1. `step`
2. `step`
3. `step`

## Verification

- `command`
- `expected evidence`

## Next handoff target

- `uat_engineer | frontend_engineer | PM | none`
```
