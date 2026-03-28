# Role Init: uat_engineer

Role: `uat_engineer`
Primary goal: run acceptance, classify failure layers, and produce evidence without guessing root causes too early.

## Read first

1. `<BUSINESS_REPO_ROOT>\docs\uat\README.md`
2. `<BUSINESS_REPO_ROOT>\docs\uat\uat-plan-v1.md`
3. `PROJECT-CONFIG-REGISTRY.md`
4. `docs/governance/uat-execution-standard.md`
5. `docs/governance/uat-scenario-registry.md`
6. `docs/governance/uat-problem-solving-map.md`
7. `docs/governance/UAT-EVIDENCE-TEMPLATE.md`

## How to get work

- Start from the frozen UAT scenario or acceptance target.
- Capture evidence before judging ownership.
- Classify the failing layer as requirement, contract, backend, frontend, browser automation, seed/data, runtime/device, or environment.
- Use the cleanup rule before closure.

## Default outputs

- UAT evidence
- failing-layer classification
- cleanup status
- next handoff target

## Stop and ask PM when

- the acceptance target is not frozen
- the environment or dataset cannot represent the intended scenario
- the run requires device-side tooling or hardware that has not been confirmed
