# File-Only Command Protocol

Status: active
Audience: PM, software engineer, embedded engineer, hardware engineer, Lovable
Purpose: let the user trigger work by saying only "execute" to a named team, while all scope, queue state, and return format stay in files.

## Workspace anchors

- development-system workspace:
  - `D:\20251211\zhinengti\development-system\projects\houjinongfuai`
- backend business-code workspace:
  - `D:\20251211\zhinengti\houjinongfuai`
- frontend workspace:
  - `D:\20251211\zhinengti\lovable`

## Core rule

- Each team has one live command file: `CURRENT.md`
- Each team has one live return file: `RESULT.md`
- Verbal restatement is optional and is not the source of truth.

Task-type hard gate:

- `CURRENT.md` must distinguish between:
  - `INTEREST`
  - `ENGINEERING`
  - `LANGUAGE`
  - `SYSTEM`
- if task type is missing, the task is not executable
- do not infer `ENGINEERING` from chat, brainstorming, or architecture discussion alone

Requirement gate:

- before dispatching executable engineering work, PM should first freeze the corresponding business requirement in the business repository
- requirement docs are human-facing artifacts in `D:\20251211\zhinengti\houjinongfuai\docs\requirements` and related business-doc folders
- task sheets in this workspace are execution artifacts, not requirement substitutes
- `definition-of-ready.md` is the minimum readiness gate before executable dispatch

Development-system evolution rule:

- development-system improvements discovered during delivery must be recorded as candidates, not silently executed as hidden scope expansion
- classify them as:
  - task type `SYSTEM`
  - shared category `development_system`
- write them back in `RESULT.md` under `development-system improvement candidates`
- PM may later freeze them into a separate executable task

## Path rule

- Treat this development-system workspace as the rule / dispatch anchor.
- Treat `D:\20251211\zhinengti\houjinongfuai` as the backend business-code anchor.
- Treat `D:\20251211\zhinengti\lovable` as the frontend workspace unless PM freezes another location in the active task.
