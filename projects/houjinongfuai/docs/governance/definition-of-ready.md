# Definition Of Ready

Status: active
Audience: PM, software engineer, Cursor, Codex, Lovable
Purpose: define the minimum requirement before a business scope can be decomposed into executable AI tasks.

## Core rule

Do not dispatch an executable AI task until the upstream business scope is ready.

Ready means the work is no longer a vague discussion item. It has enough business truth to be executed without guessing core intent.

## Minimum ready checklist

Before creating an executable task, PM should confirm all of the following:

1. requirement source exists
   - a business-facing requirement document exists in the business repository
2. scope is frozen enough for execution
   - what is in scope and out of scope is clear enough to implement or verify
   - if this is a requirement change, global impact points are explicitly marked
3. intent is explicit
   - the work is clearly one of:
     - `INTEREST`
     - `ENGINEERING`
     - `LANGUAGE`
     - `SYSTEM`
4. task category is explicit
   - the work is clearly one of:
     - business delivery
     - acceptance collaboration
     - development-system/tooling
5. execution mode is explicit
   - for software engineer tasks:
     - `BACKEND`
     - `SYNC`
     - `VERIFY`
6. owner role is explicit
   - for example:
     - `requirements_engineer`
     - `feature_research_engineer`
     - `software_engineer`
     - `frontend_engineer`
     - `uat_engineer`
7. allowed working area is explicit
   - which repo or directory may be touched
8. verification basis is explicit
   - build, unit test, e2e, local acceptance, sync confirmation, or docs-only review
9. handoff target is explicit
   - who receives the result next

If any of these are still ambiguous, the task is not ready for executable dispatch.

## Required dispatch fields

Every executable task should provide at least:

1. `task id`
2. `requirement source`
3. `task type`
4. `task category`
5. `owner role`
6. `mode`
7. `status`
8. `allowed working area`
9. `execute now`
10. `verification`
11. `next handoff target`
12. `global impact points` when the task is derived from a requirement change

## Examples

### Ready example

- requirement source:
  - a requirement doc linked from `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
- task type:
  - `ENGINEERING`
- task category:
  - `business_delivery`
- mode:
  - `BACKEND`
- allowed working area:
  - backend only
- verification:
  - `npm run build`
  - affected tests

This is ready.

### Not ready example

- requirement source is missing
- task says only "keep improving this"
- no mode
- no verification basis

This is not ready.

## Rule for discussions

Discussion can still be recorded and tracked.

But until the item meets the ready checklist:

- keep it as `INTEREST` or `SYSTEM`
- do not dispatch it as executable `ENGINEERING`
