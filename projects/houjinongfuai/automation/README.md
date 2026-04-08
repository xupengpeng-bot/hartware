# Automation README

Status: active
Audience: PM, development_system_engineer, AI runtime maintainers
Purpose: keep automation runtimes and launch assets in one place so the project root does not become cluttered.

## Rule

Automation integrations should live under this directory.

Do not scatter runtime-specific prompts, launch contracts, and helper scripts across multiple top-level folders unless they are part of the core live entry chain.

## Current integrations

- `openhands`
  - OpenHands adaptation plan
  - launch profile
  - pilot prompt
  - prelaunch checker
  - goal-attainment review

## Scope split

- `docs/codex`
  - smallest live entry chain
  - dispatch state
  - role-init cards
- `docs/governance`
  - project-wide process and quality rules
- `automation`
  - runtime-specific integration assets
