# Domain Navigation

Status: active
Audience: PM, AI agents
Purpose: tell a new thread where to look when the task touches a domain that is not yet obvious.

## Rule

If the domain is unclear, do not guess from old history first.

Start from this map, then move into the matching active source.

## Domain map

### Business requirements and product truth

Read:

- `<BUSINESS_REPO_ROOT>\docs\README.md`
- `<BUSINESS_REPO_ROOT>\docs\requirements\README.md`
- the business system-docs README under `<BUSINESS_REPO_ROOT>\docs`

Use when:

- the request is still fuzzy
- the requirement meaning is unclear
- feature scope or acceptance logic needs clarification

### Software-engineering execution

Read:

- `CURRENT.md`
- `TASK-TYPES.md`
- `WORK-MODES.md`
- the active task file linked from `CURRENT.md`

Use when:

- the task is already frozen as executable engineering

### Frontend sync and verify

Read:

- `<BUSINESS_REPO_ROOT>\docs\uat\lovable-codex-sync.md`
- frontend task packages or handoff notes
- `<FRONTEND_REPO_ROOT>\lovablecomhis\CURRENT.md` when PM explicitly opens frontend coordination

Use when:

- the task mode is `SYNC` or `VERIFY`

### UAT and acceptance

Read:

- `<BUSINESS_REPO_ROOT>\docs\uat\README.md`
- `<BUSINESS_REPO_ROOT>\docs\uat\uat-plan-v1.md`
- `docs/governance/uat-execution-standard.md`
- `docs/governance/uat-scenario-registry.md`
- `docs/governance/UAT-EVIDENCE-TEMPLATE.md`

Use when:

- the task is about acceptance evidence, scenario execution, closure, or cleanup

### Device protocol, runtime simulation, embedded, hardware

Read:

- `docs/governance/device-simulation-registry.md`
- `docs/governance/uat-problem-solving-map.md`
- `<BUSINESS_REPO_ROOT>\embeddedcomhis\CURRENT.md`
- `<BUSINESS_REPO_ROOT>\hardwarecomhis\CURRENT.md`

Use when:

- the task touches protocol packets, simulated device reporting, firmware, flashing, hardware interface, or serial observation

### Marketing and business materials

Read:

- `PROJECT-MARKETING-BRIEF.md`
- `docs/governance/marketing-strategy-standard.md`
- `docs/governance/MARKETING-MATERIAL-TEMPLATE.md`

Use when:

- the request is about external messaging, pitch materials, differentiated strengths, or audience-specific communication

### Development-system and workflow governance

Read:

- `ROLE-INIT-README.md`
- `docs/governance/README.md`
- `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\role-lane-catalog.md`
- `<DEVSYSTEM_REPO_ROOT>\shared\global-rules\development-system-evolution-rule.md`

Use when:

- the task is about process, onboarding, templates, preflight, or workflow improvement

### Dispatch DB and bootstrap

Read:

- `REMOTE-FIRST-BOOTSTRAP.md`
- `CURRENT-DB-BOOTSTRAP-TEMPLATE.md`
- `docs/governance/dispatch-db-pilot.md`
- `<BUSINESS_REPO_ROOT>\backend\scripts\dispatch_bootstrap_fetch.py`

Use when:

- the machine is new
- the local workspace does not exist yet
- the live task state should come from dispatch DB

### Historical tasks and old delivery context

Read:

- `docs/codex/history/README.md`
- `docs/codex/history/<period>/README.md`
- the specific archived task sheet only when PM or the active task points to it

Use when:

- you need old execution evidence or past task decomposition
- you are checking how a prior wave was executed

## Unknown-domain rule

If the task still does not fit any domain:

1. classify it as likely `INTEREST` or `SYSTEM`
2. route it to `requirements_engineer`, `feature_research_engineer`, or `development_system_engineer`
3. do not silently convert it into executable engineering
