# Device Simulation Registry

Status: active
Audience: PM, software engineer, embedded engineer, UAT engineer, AI agents
Purpose: centralize where runtime and device-report simulations should be looked up.

## Core rule

If a scenario depends on packets, reconnects, duplicate reports, order settlement side effects, or missing stop events, check this registry before inventing a new simulation path.

## Standard fields

- scenario id
- scenario name
- current primary layer
- primary owner role
- current execution asset
- fallback execution asset
- notes

## Active registry

| Scenario ID | Scenario Name | Primary Layer | Owner Role | Current Execution Asset | Fallback Asset | Notes |
|---|---|---|---|---|---|---|
| SIM-RT-001 | duplicate packet | backend runtime simulation | `software_engineer` | backend integration/e2e support | later embedded simulator fixture | should validate idempotency |
| SIM-RT-002 | out-of-order packet | backend runtime simulation | `software_engineer` | backend integration/e2e support | later embedded simulator fixture | should validate state-machine safety |
| SIM-RT-003 | disconnect and reconnect | backend runtime simulation | `software_engineer` | backend integration/e2e support | later embedded simulator fixture | should validate reconnect ownership and session continuity |
| SIM-RT-004 | power loss without stop | runtime/device dependent | `embedded_engineer` + `software_engineer` | backend scenario support plus handoff notes | future dedicated simulator script | should validate auto-end and audit behavior |
| SIM-RT-005 | alarm during runtime | runtime/device dependent | `embedded_engineer` + `software_engineer` | backend scenario support plus handoff notes | future dedicated simulator script | should validate alert and billing interaction |

## Current limitation note

The project already has:

- scenario definitions in `docs/uat/uat-plan-v1.md`
- backend integration and e2e tests
- embedded and hardware handoff folders

But it does not yet have one fully standardized executable simulator package for all device scenarios.

Treat that as a known system limitation, not as permission to guess.
