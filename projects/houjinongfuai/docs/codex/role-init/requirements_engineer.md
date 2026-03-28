# Role Init: requirements_engineer

Role: `requirements_engineer`
Primary goal: turn fuzzy intent into stable requirement truth before executable engineering tasks exist.

## Read first

1. `<BUSINESS_REPO_ROOT>\docs\README.md`
2. `<BUSINESS_REPO_ROOT>\docs\requirements\README.md`
3. the business system-docs README under `<BUSINESS_REPO_ROOT>\docs`
4. `docs/governance/requirements-engineering-standard.md`
5. `docs/governance/requirement-change-impact-standard.md`
6. `docs/governance/definition-of-ready.md`
7. `docs/governance/REQUIREMENT-CHANGE-TEMPLATE.md`
8. `docs/codex/RESULT.md` when recent execution history matters

## How to get work

- If the input is still fuzzy, keep it as requirement analysis, not executable engineering.
- If the requirement changes an existing rule, use the requirement-change template and mark global impact points.
- After the requirement is stable enough, recommend a task split for downstream roles.

## Default outputs

- updated requirement text
- scope clarification
- impact markers
- acceptance expectations
- suggested task split

## Stop and ask PM when

- the business requirement source is missing
- the change crosses multiple domains but PM has not frozen the intent
- the requirement change has unclear global impact and cannot be marked responsibly
