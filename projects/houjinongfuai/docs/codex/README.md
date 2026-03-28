# Codex Docs

Status: active
Audience: PM and software engineer
Purpose: describe the live software-engineer execution entry and fixed work modes for the external development-system workspace.

## Workspace anchors

- development-system workspace:
  - `D:\20251211\zhinengti\development-system\projects\houjinongfuai`
- business-code workspace:
  - `D:\20251211\zhinengti\houjinongfuai`
- frontend workspace:
  - `D:\20251211\zhinengti\lovable`

## Files

Live software-engineer work is dispatched through this development-system workspace:

- `PROJECT-CONFIG-REGISTRY.md`
- `PROJECT-MARKETING-BRIEF.md`
- `docs/codex/CURRENT.md`
- `docs/codex/RESULT.md`
- `docs/codex/TASK-DISPATCH-TEMPLATE.md`
- `docs/codex/ROLE-INIT-README.md`
- `docs/codex/REMOTE-FIRST-BOOTSTRAP.md`
- `docs/codex/WORK-MODES.md`
- `docs/codex/TASK-TYPES.md`
- `docs/codex/CLOUD-DEVELOPMENT-WORKFLOW.md`
- `D:\20251211\zhinengti\development-system\shared\global-rules\role-lane-catalog.md`
- `docs/governance/file-only-command-protocol.md`
- `docs/governance/delivery-workflow.md`
- `docs/governance/requirements-engineering-standard.md`
- `docs/governance/requirement-change-impact-standard.md`
- `docs/governance/role-based-delivery-model.md`
- `docs/governance/marketing-strategy-standard.md`
- `docs/governance/REQUIREMENT-CHANGE-TEMPLATE.md`
- `docs/governance/UAT-EVIDENCE-TEMPLATE.md`
- `docs/governance/MARKETING-MATERIAL-TEMPLATE.md`
- `docs/governance/uat-execution-standard.md`
- `docs/governance/uat-scenario-registry.md`
- `docs/governance/device-simulation-registry.md`
- `docs/governance/uat-problem-solving-map.md`
- `D:\20251211\zhinengti\development-system\shared\global-rules\encoding-governance.md`
- `D:\20251211\zhinengti\development-system\shared\global-rules\development-system-evolution-rule.md`
- `D:\20251211\zhinengti\development-system\shared\global-rules\project-config-standard.md`
- `D:\20251211\zhinengti\development-system\shared\global-rules\windows-path-governance.md`

Preflight helper:

- `tools/preflight.ps1`
- `tools/bootstrap-from-remote.ps1`

Business sync note still lives with the codebase:

- `D:\20251211\zhinengti\houjinongfuai\docs\uat\lovable-codex-sync.md`

## Historical task rule

- `CURRENT.md`, `RESULT.md`, and the shared/global rules are the active entry chain.
- `COD-*.md` files in this folder are historical task sheets.
- Do not read historical task sheets unless `CURRENT.md` or PM explicitly points to one.
- Use the template files when creating new tasks or reusable evidence instead of copying historical task sheets.

## Role-init rule

- Use `ROLE-INIT-README.md` and the `role-init/*` cards when opening a fresh AI thread for a specific role.
- Role-init cards do not replace `CURRENT.md`; they explain how each role should obtain work and where to stop.

When PM or the user says only "execute", software engineer must read this folder's `CURRENT.md` first.
