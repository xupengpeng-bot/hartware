# Requirements To Tasks Rule

Status: active
Audience: PM, software engineer, Cursor, Codex, Lovable
Purpose: separate human-facing business requirements from AI-facing execution tasks.

## Core rule

Business requirements and AI task instructions are not the same artifact.

They must stay physically and logically separate.

## Where each artifact belongs

Business requirement docs belong in the business repository:

- `D:\20251211\zhinengti\houjinongfuai\docs\requirements`
- `D:\20251211\zhinengti\houjinongfuai\docs\系统说明`
- `D:\20251211\zhinengti\houjinongfuai\docs\p1`
- `D:\20251211\zhinengti\houjinongfuai\docs\protocol`
- `D:\20251211\zhinengti\houjinongfuai\docs\uat`

AI task instructions belong in the development-system workspace:

- `D:\20251211\zhinengti\development-system\projects\houjinongfuai\docs\codex`
- `D:\20251211\zhinengti\development-system\projects\houjinongfuai\docs\governance`

## Intent split

Business requirement docs are for humans:

- overall business goals
- feature scope
- object model
- functional points
- protocol rules
- acceptance expectations

AI task instructions are for execution:

- current active task
- task type
- mode
- allowed working area
- execute-now instruction
- writeback format

## Required workflow

The correct order is:

1. capture fuzzy product intent if the demand is still rough
2. confirm or update business requirements in the business repository
3. if the requirement was changed, mark the global impact surface explicitly
4. freeze the requirement scope
5. decompose the confirmed scope into typed AI tasks in the development-system workspace
6. execute
7. verify
8. if needed, write requirement-level updates back into the business repository

## Hard gate

Do not dispatch an executable AI engineering task when the upstream business requirement is still ambiguous.

Do not use task sheets as a substitute for requirement documents.

Do not mix requirement prose and execution instructions into one file unless PM explicitly marks it as a temporary bridge artifact.
