# Root Workspace Map

Status: active
Audience: PM, engineers, AI agents
Purpose: define which local directories are the current active truth, which are sidecar, and which are scratch-only.

## Root

- workspace root:
  - `D:\Develop\houji\houjinongfuAI-Cursor`

## Current classification

### Formal active repositories

These are the default active repositories for the current local delivery chain.

- `D:\Develop\houji\houjinongfuAI-Cursor\hartware`
  - role: embedded firmware and device-side protocol truth
  - status: formal active
- `D:\Develop\houji\houjinongfuAI-Cursor\houjinongfuai-working`
  - role: backend and formal business/protocol documents
  - status: formal active
- `D:\Develop\houji\houjinongfuAI-Cursor\lovable-working`
  - role: main frontend workspace
  - status: formal active

### Former sidecar (removed from workspace root)

- The standalone `waterflow-control` demo repository is **not** cloned under this workspace root anymore (removed 2026-04-16).
- Network / scheduling UI prototypes live in `lovable-working/src/features/waterflow/`.
- Historical notes: `houjinongfuai-working/docs/附属项目-waterflow-control.md` (archived).

### Parallel sibling clones

These are valid repositories, but they are not the default truth for this workspace container.

- `D:\Develop\houji\houjinongfuai`
  - role: parallel backend clone
  - status: reference only by default
- `D:\Develop\houji\lovable`
  - role: parallel frontend clone
  - status: reference only by default
- `D:\Develop\houji\waterflow-control`
  - role: parallel sidecar clone (if still present)
  - status: reference only by default

### Temporary or scratch-only directories

These must not be treated as default active truth.

- `D:\Develop\houji\houjinongfuAI-Cursor\tmp`
  - role: temporary workspace artifacts
  - status: local-only scratch
- `D:\Develop\houji\houjinongfuAI-Cursor\ota-pub`
  - role: local publication/output helper directory
  - status: local-only helper

## Current judgement

For the current round:

1. use the three repositories inside `D:\Develop\houji\houjinongfuAI-Cursor` (`hartware`, `houjinongfuai-working`, `lovable-working`) as the default local truth
2. treat sibling clones under `D:\Develop\houji\` as reference unless the task explicitly switches to them
3. keep temporary logs, screenshots, scratch scripts, and comparison outputs under `tmp/`

## Embedded-specific rule

For embedded protocol, OTA, and device behavior:

1. default truth is the current local `D:\Develop\houji\houjinongfuAI-Cursor\hartware`
2. product-specific valve-control work defaults to:
   - `D:\Develop\houji\houjinongfuAI-Cursor\hartware\products\SCAN-IRR-VALVE-CTRL-4G-A01`
3. remote repositories and historical branches are reference only unless explicitly requested

## AI read rule

Unless the current task explicitly says otherwise, AI should read only:

1. root docs under `D:\Develop\houji\houjinongfuAI-Cursor`
2. `D:\Develop\houji\houjinongfuAI-Cursor\houjinongfuai-working`
3. `D:\Develop\houji\houjinongfuAI-Cursor\lovable-working` when frontend work is active
4. `D:\Develop\houji\houjinongfuAI-Cursor\hartware` for embedded work

AI must not treat sibling clones or `tmp/` as default truth sources.
