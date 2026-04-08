# UAT Evidence Template

Status: active-template
Audience: UAT engineer, PM, software engineer
Purpose: keep UAT evidence and failure classification stable across runs and AI agents.

## Template

```md
# UAT Evidence: Case ID / Title

## Environment

- environment:
- operator:
- execution time:

## Scenario

- scenario id:
- scenario layer:
- preconditions:

## Steps executed

1. `step`
2. `step`
3. `step`

## Observed result

- `what happened`

## Expected result

- `what should have happened`

## Evidence

- screenshot:
- trace/video:
- backend log:
- API response:

## Failing layer classification

- `requirement | contract | backend | frontend | browser-automation | seed/data | runtime/device | environment`

## Cleanup status

- `done | not needed | blocked`

## Next handoff target

- `software_engineer | frontend_engineer | embedded_engineer | PM`
```
