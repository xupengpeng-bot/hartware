# Document System

Status: active
Audience: PM, Codex, Lovable, embedded, hardware, QA
Purpose: keep documentation clean, layered, and unambiguous.

## Levels

### L0 Baseline

These define project scope and the highest-level frozen truths.

- `AGENTS.md`
- `docs/p1/*`
- `docs/protocol/*`

### L1 Contract

These define current system behavior, domain freeze, and integration truth.

- `docs/uat/frontend-backend-contract-checklist-v1.md`
- `docs/uat/domain-region-project-asset-device-v2.md`
- `docs/uat/relation-model-v2.md`
- `docs/uat/location-model-v1.md`
- `docs/uat/domain-compat-mapping-v1.md`
- `backend/openapi.v1.yaml`

### L2 Collaboration

These define current task flow and team coordination.

- `docs/uat/lovable-codex-sync.md`
- `docs/governance/*`
- `lovablecomhis/*` in the frontend repo
- `embeddedcomhis/*`
- `hardwarecomhis/*`

### L3 Archive

These are preserved for traceability, but they are not live truth.

- `docs/archive/*`

## Cleanup rules

- Duplicate prototype docs must be moved to `docs/archive`, not left in active folders.
- Historical task sheets must be archived once the project has moved past that wave.
- If a document can mislead active implementation, it must either be updated or archived.
- Active folders should contain only documents that a current contributor is allowed to trust.

## Metadata rule

Every new active doc should start with:

- `Status`
- `Audience`
- `Purpose`

Optional but recommended:

- `Scope`
- `Last reviewed`

## Current cleanup result

The following items have been moved out of active folders because they were duplicate, exploratory, or stale:

- old prototype route maps
- old four-role prototype UAT script
- old Codex task sheet
- old exploratory CRUD matrix / field dictionary / form contract set
- old region contract task notes

Use `docs/archive/README.md` to understand what stayed and why.
