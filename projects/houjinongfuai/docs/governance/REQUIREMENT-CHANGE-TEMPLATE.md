# Requirement Change Template

Status: active-template
Audience: PM, requirements engineer
Purpose: make requirement changes explicit before delivery tasks are decomposed.

## When to use

Use this template when an existing requirement changes behavior, field meaning, flow, scope, or acceptance logic.

## Template

```md
# Requirement Change: Short Title

## Change summary

- `what changed`

## Reason for change

- `why the requirement changed`

## Changed requirement sources

- `absolute/path/to/business-doc.md`

## Global impact points

1. business object model
   - `impacted | not impacted | unknown`
   - note:
2. backend API or response contract
   - `impacted | not impacted | unknown`
   - note:
3. frontend page, form, or selector behavior
   - `impacted | not impacted | unknown`
   - note:
4. database schema, seed, or baseline data
   - `impacted | not impacted | unknown`
   - note:
5. UAT scenario or acceptance criteria
   - `impacted | not impacted | unknown`
   - note:
6. browser automation or local verification flow
   - `impacted | not impacted | unknown`
   - note:
7. runtime, billing, audit, or session logic
   - `impacted | not impacted | unknown`
   - note:
8. protocol simulation or device-side assumptions
   - `impacted | not impacted | unknown`
   - note:
9. embedded or hardware handoff
   - `impacted | not impacted | unknown`
   - note:

## Non-impacted areas

- `explicitly list safe areas`

## Required follow-up roles

- `requirements_engineer`
- `software_engineer`
- `uat_engineer`

## Acceptance impact

- `what must be re-verified`

## Suggested task split

- `task candidate 1`
- `task candidate 2`
```
