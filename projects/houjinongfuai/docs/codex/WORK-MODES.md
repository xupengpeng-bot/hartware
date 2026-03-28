# Codex Work Modes

Status: active
Audience: PM, software engineer
Purpose: reduce dispatch ambiguity by separating software-engineer work into fixed modes.

Important:

- work mode is not the same thing as task type
- task type answers "what kind of task is this"
- work mode answers "what should the software engineer do right now"
- read `docs/codex/TASK-TYPES.md` before interpreting `BACKEND`, `SYNC`, or `VERIFY`

## Path baseline

- Treat this development-system workspace as the rule / dispatch workspace.
- Treat `D:\20251211\zhinengti\houjinongfuai` as the backend business-code workspace.
- Treat `D:\20251211\zhinengti\lovable` as the frontend workspace unless a task file explicitly freezes another path.

## Modes

### `SYNC`

- software engineer works only as the local Git synchronization operator
- no frontend business behavior should change in this mode

### `VERIFY`

- software engineer works only as the local pull + acceptance verifier
- do not use stale local code as the final basis for rejection

## Dispatch rule

When PM activates a software-engineer task, the task file and `CURRENT.md` should always make both explicit:

- task type:
  - `INTEREST`
  - `ENGINEERING`
  - `LANGUAGE`
  - `SYSTEM`
- mode:
  - `BACKEND`
  - `SYNC`
  - `VERIFY`

Only `ENGINEERING` tasks should normally enter `BACKEND`, `SYNC`, or `VERIFY`.
