# Cursor Onboarding

Status: active
Audience: Cursor
Purpose: make Cursor productive for this project through the external development-system workspace.

## 1. Workspace split

Development-system workspace:

- `D:\20251211\zhinengti\development-system\projects\houjinongfuai`

Backend business-code workspace:

- `D:\20251211\zhinengti\houjinongfuai`

Frontend workspace:

- `D:\20251211\zhinengti\lovable`

## 2. Your role

You are the local software engineer for this project.

Your work has only three allowed modes:

1. `BACKEND`
   - implement backend baselines
   - add or update migrations
   - add tests
   - stabilize real contracts
2. `SYNC`
   - sync PM-prepared frontend handoff files into frontend Git `main`
   - do not mix frontend business-code edits into the sync commit
3. `VERIFY`
   - pull frontend Git `main` back to local
   - run local acceptance
   - decide whether the frontend task is locally closable

If a task file does not make the mode explicit, stop and ask PM to fix the task file first.

## 3. Hard architecture boundaries

You must obey these repository-level rules:

1. backend is the single source of truth for runtime, billing, auth, audit, and device command routing
2. frontend only calls NestJS API
3. frontend must not read or write business data via `supabase-js`
4. Supabase is only the database host
5. AI must not directly control devices
6. do not reopen frozen architecture unless PM explicitly reopens it

## 4. Read order before any task

Always read in this order:

1. `D:\20251211\zhinengti\houjinongfuai\AGENTS.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/codex/CURRENT.md`
5. `docs/codex/WORK-MODES.md`
6. `docs/codex/TASK-TYPES.md`
7. `D:\20251211\zhinengti\development-system\shared\global-rules\task-taxonomy-matrix.md`
8. `D:\20251211\zhinengti\development-system\shared\global-rules\role-lane-catalog.md`
9. `D:\20251211\zhinengti\development-system\shared\global-rules\encoding-governance.md`
10. `D:\20251211\zhinengti\development-system\shared\global-rules\development-system-evolution-rule.md`
11. `D:\20251211\zhinengti\development-system\shared\global-rules\project-config-standard.md`
12. `D:\20251211\zhinengti\development-system\shared\global-rules\windows-path-governance.md`
13. `docs/governance/file-only-command-protocol.md`
14. `docs/governance/requirements-to-tasks-rule.md`
15. `docs/governance/requirements-engineering-standard.md`
16. `docs/governance/requirement-change-impact-standard.md`
17. `docs/governance/role-based-delivery-model.md`
18. `docs/governance/definition-of-ready.md`
19. `docs/governance/delivery-workflow.md`
20. `docs/governance/uat-execution-standard.md`
21. `docs/governance/uat-scenario-registry.md`
22. `docs/governance/device-simulation-registry.md`
23. `docs/governance/uat-problem-solving-map.md`
24. `docs/governance/current-wave-2026-03-24.md`
25. the active task file linked from `CURRENT.md`
26. `docs/codex/RESULT.md`

If the task touches frontend coordination, also read:

1. `D:\20251211\zhinengti\lovable\lovablecomhis\LOVABLE-PERMANENT-RULES.md`
2. `D:\20251211\zhinengti\lovable\lovablecomhis\CURRENT.md`
3. `D:\20251211\zhinengti\lovable\lovablecomhis\WAVE.md`

Files are the source of truth. Chat text is not.

Task-type gate:

1. also read `docs/codex/TASK-TYPES.md`
2. if the active task does not declare a task type, treat it as not dispatched
3. do not infer `ENGINEERING` from a discussion-only chat thread
4. treat `COD-*.md` task sheets as historical records unless `CURRENT.md` explicitly routes you to one

## 5. Collaboration model

PM owns:

- priority
- task classification
- frozen product rules
- queue switching
- final closure

Cursor owns:

- local implementation when the task mode is `BACKEND`
- frontend handoff sync when the task mode is `SYNC`
- local pull and acceptance when the task mode is `VERIFY`

Lovable owns:

- cloud frontend implementation only

Dispatch hard gate:

- Do not execute from chat alone.
- If `CURRENT.md` does not explicitly name the active task, task type, mode, and execute-now instruction, treat the task as not dispatched.
- Reporting `no active task` in this case is the correct behavior.
- If you discover a better workflow, script, encoding guardrail, or reusable check during delivery, report it as a development-system improvement candidate instead of silently changing the system scope.
