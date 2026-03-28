# UAT Scenario Registry

Status: active
Audience: PM, UAT engineer, software engineer, AI agents
Purpose: keep UAT scenarios in one stable registry instead of scattering them across chat or memory.

## Usage rule

Use this file as the scenario index.

Detailed evidence may live elsewhere, but scenario identity, layer, and expected method should be listed here.

## Standard fields

- scenario id
- scenario name
- layer
- owner role
- execution surface
- required truth sources
- expected evidence
- cleanup required

## Active scenario registry

| Scenario ID | Scenario Name | Layer | Owner Role | Execution Surface | Required Truth Sources | Expected Evidence | Cleanup Required |
|---|---|---|---|---|---|---|---|
| UAT-BE-001 | Backend contract smoke | backend automated | `software_engineer` | `npm run test:e2e` / smoke tests | UAT docs + backend tests | passing test output and summary | yes |
| UAT-BR-001 | Browser local acceptance | browser acceptance | `uat_engineer` | local browser or Playwright | UAT plan + contract checklist + active frontend task | screenshots, trace, result summary | yes |
| UAT-RT-001 | Runtime ingest scenario verification | runtime simulation | `software_engineer` or `uat_engineer` | backend integration/e2e and scenario support | UAT plan + integration tests + seed scenarios | state transition evidence | yes |
| UAT-DV-001 | Real device or protocol dependent verification | device dependent | `embedded_engineer` + `uat_engineer` | embedded/hardware handoff plus local validation | embedded/hardware handoff + UAT plan | firmware log / device evidence / closure note | yes |

## Expansion rule

When a new UAT scenario is added:

1. register it here first
2. link the detailed execution asset later
3. do not leave important UAT scenarios only in chat
