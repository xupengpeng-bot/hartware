# Root Workspace Map

Status: active
Audience: PM, engineers, AI agents
Purpose: define which root-level workspaces are formal, which are auxiliary, and which must not be treated as active truth by default.

## Root

- workspace root:
  - `D:\20251211\zhinengti`

## Current classification

### Formal active workspaces

These are part of the current active delivery chain and should stay stable during the current verification round.

- `D:\20251211\zhinengti\houjinongfuai`
  - role: business-code repository
  - status: formal active
- `D:\20251211\zhinengti\lovable`
  - role: formal frontend repository
  - status: formal active
- `D:\20251211\zhinengti\development-system`
  - role: development-system repository
  - status: formal active

### Research or sidecar workspaces

These may be useful, but they are not the default truth source for the current business mainline.

- `D:\20251211\zhinengti\external\waterflow-control`
  - role: external or research project
  - status: sidecar

### Temporary or verification-only workspaces

These must not be treated as default active truth.

- `D:\20251211\zhinengti\lovable-accept-4012`
  - role: temporary acceptance or verification clone
  - status: temporary
  - note: currently not on a normal branch (`HEAD detached`)

### Local utility or scratch workspaces

These are not active Git truth sources for the current delivery chain.

- `D:\20251211\zhinengti\analyze`
  - role: local analysis or scratch directory
  - status: local-only
- `D:\20251211\zhinengti\codexAgaent`
  - role: local tool or scratch directory
  - status: local-only

## Current judgement

For the current round, do not move the three formal active workspaces:

1. `houjinongfuai`
2. `lovable`
3. `development-system`

Moving them now would create more risk than value because:

- scripts and onboarding are already aligned to these locations
- the new-machine environment has just been stabilized
- verification should not be interrupted by directory migration

## What should be adjusted now

Adjust classification, not physical paths.

Immediate recommendation:

1. keep `houjinongfuai`, `lovable`, and `development-system` exactly where they are for this round
2. treat `lovable-accept-4012` as temporary and do not let AI read it by default
3. treat `external\waterflow-control` as research or sidecar only
4. treat `analyze` and `codexAgaent` as local scratch areas, not delivery truth

## What can wait until after verification

After this verification round, root-level cleanup can be done safely.

Recommended future target structure:

- `projects\`
  - formal business projects
- `systems\`
  - development-system and shared automation
- `labs\`
  - research and external explorations
- `scratch\`
  - temporary clones, acceptance copies, and throwaway local work

## AI read rule

Unless PM explicitly dispatches otherwise, AI should read only:

1. `D:\20251211\zhinengti\houjinongfuai`
2. `D:\20251211\zhinengti\development-system`
3. `D:\20251211\zhinengti\lovable` when the active task requires frontend `SYNC` or `VERIFY`

AI must not treat `lovable-accept-4012`, `external`, `analyze`, or `codexAgaent` as default truth sources.
