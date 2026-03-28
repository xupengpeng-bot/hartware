# Cursor Task Card

```text
Cursor task card

Task ID:
Task title:
Work mode: BACKEND / SYNC / VERIFY
Priority: P0 / P1 / P2

1. Read these files first
1. ./AGENTS.md
2. ./docs/codex/CURSOR-ONBOARDING.md
3. ./docs/系统说明/通用产品规则.md
4. ./docs/系统说明/系统业务总览简版.md
5. ./docs/系统说明/系统整体业务需求.md
6. ./docs/系统说明/需求拆解.md
7. ./docs/codex/CURRENT.md
8. ./docs/codex/WORK-MODES.md
9. ./docs/governance/file-only-command-protocol.md
10. ./docs/governance/delivery-workflow.md
11. the current task file
12. ./docs/codex/RESULT.md

Frontend repo rule:
- default frontend repo path is ../lovable
- if the task freezes another path, follow the task file

2. Task goal
-

3. Scope
In scope:
-

Out of scope:
-

4. Execution rules
1. Only execute the current task pointed to by CURRENT.md.
2. Files are the source of truth. Do not guess from old chat context.
3. If CURRENT.md does not contain active task / work mode / execute-now, treat the task as not dispatched and stop.
4. Follow the work mode strictly:
   - BACKEND: backend implementation, migrations, tests, contract stabilization
   - SYNC: sync only frontend handoff files into frontend Git main
   - VERIFY: local pull, build, and acceptance only; do not patch missing behavior locally
5. Do not mix SYNC and VERIFY.
6. Frontend may call only NestJS API.
7. Frontend must not directly call third-party business or geoservice endpoints unless PM freezes an exception.
8. Do not expand scope or reopen architecture.

5. Working area
Backend repo:
- current repository root

Frontend repo:
- ../lovable

Allowed read/write set:
-

6. Acceptance criteria
1.
2.
3.

7. VERIFY hard gate
Before outputting any conclusion such as "not complete", "still broken", or "not closable", first check:
1. git rev-parse HEAD
2. git rev-parse origin/main
3. git status --short

Rules:
1. If local HEAD is behind origin/main, do not reject the task from stale local code.
2. If the local workspace is dirty, do not treat it as remote-main truth.
3. If the local repo is behind or dirty, mark verification risk first and explain that re-check on the latest clean snapshot is required.
4. PM local stale workspace is not final rejection evidence; final rejection must be based on remote main or a clean local snapshot after pull.

8. Feedback format
1. task id
2. mode
3. status: fixed / blocked / paused / waiting_sync / waiting_verify / done_without_change
4. changed files or synced files
5. verification result
6. commit SHA or no git action
7. pending issues
8. next handoff target

9. VERIFY must also include
1. pulled frontend HEAD commit SHA
2. origin/main commit SHA
3. git status --short summary
4. local build result
5. whether the task is closable

10. Logic-loop stop rule
If any of the following happens, stop and report logic-loop risk immediately:
1. the same task is dispatched again with no file-state change
2. CURRENT.md and the queue board disagree
3. chat says active but files do not
4. PM already reopened the task but someone still asks for old verification
5. the files you are asked to sync are already present in remote main
6. local HEAD is behind remote main but someone still asks for a final rejection from stale code
```
