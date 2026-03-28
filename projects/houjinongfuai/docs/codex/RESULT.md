# Codex Result

Status: active-template
Audience: Codex and PM
Purpose: overwrite the latest-result section after each execution. Keep the field order stable.

## Required format

1. execution time
2. task id
3. mode
4. status
5. changed files
6. migration or contract summary
7. verification result
8. frontend impact
9. commit SHA or `no git action`
10. pending issues
11. next handoff target
12. development-system improvement candidates

## Latest result

- execution time
  - `YYYY-MM-DD HH:mm`
- task id
  - `TASK-ID`
- mode
  - `BACKEND | SYNC | VERIFY | n/a`
- status
  - `done | blocked | partial | no active task`
- changed files / synced files
  - `path/or/note`
- migration or contract summary
  - `none` or short summary
- verification result
  - `command -> pass/fail`
- frontend impact
  - `none` or short note
- commit SHA or `no git action`
  - `no git action`
- pending issues
  - `none`
- next handoff target
  - `owner-role-or-team`
- development-system improvement candidates
  - `none`
