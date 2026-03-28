# Task System Architecture V1

Status: proposed
Audience: PM, software engineer, Lovable, embedded, hardware, QA
Purpose: define a database-backed task system that keeps a minimal local bootstrap file, supports fine-grained task classification, and scales to multi-user collaboration with full traceability.

## 1. Design goals

This architecture is designed to solve the next-stage coordination problems in this project:

- task types are becoming too mixed in file names alone
- different kinds of work need different handling:
  - interest / exploratory questions
  - formal engineering delivery
  - language / writing / translation work
  - development-system and workflow adjustments
- one active file per team is easy to start with, but hard to classify, query, audit, and scale
- more than one human or AI role must collaborate on the same work item
- more than one project or repo line may be active at the same time
- every task should be queryable by type, owner, state, wave, repo, and trace chain

The target model is:

- a very small local bootstrap file remains
- the database becomes the task system of record
- files become onboarding and fallback material, not the only live execution truth

## 2. Core decision

Do not choose between "all files" and "all database".

Use a two-layer model:

- local bootstrap layer
  - tiny, human-readable, AI-friendly
  - tells an agent where the live task state is
- database task layer
  - stores task metadata, versions, state flow, assignment, audit trail, artifacts, and results

This keeps startup simple and makes long-term collaboration manageable.

## 2.1 Multi-user and multi-project statement

Yes, this architecture is intended to support both:

- many users collaborating on one task
- many projects sharing one task system

But to do that cleanly, "user" and "project" must both be first-class entities.

That means:

- do not bind tasks only to a file path
- do not bind tasks only to one team lane
- do not assume one task belongs to exactly one repo forever

Instead:

- users belong to teams and may participate in many projects
- tasks may have one primary project and multiple related projects
- a task system must support both project-local tasks and cross-project tasks

## 2.2 Two kinds of collaboration that must be modeled separately

### People collaboration

This is about:

- who created the task
- who owns it now
- who is assigned
- who reviews
- who verifies
- who participates

### Project collaboration

This is about:

- which project the task primarily belongs to
- whether it also impacts other projects
- which repo and workspace the task lives in
- whether the task is a shared platform / governance task

If you only model the people side and not the project side, multi-project operation will still break down.

## 3. Two-layer architecture

### 3.1 Local bootstrap layer

Keep one minimal entry file per execution lane, for example:

- `docs/codex/CURRENT.md`
- `../lovable/lovablecomhis/CURRENT.md`
- `embeddedcomhis/CURRENT.md`
- `hardwarecomhis/CURRENT.md`

These files should become much smaller than today. They should only contain:

- current mode
- active task id
- lane / team
- source of truth = dispatch DB
- fallback rule if DB is unavailable
- explicit do-not-guess rule

Example:

```text
status: active
team: software_engineer
active task id: COD-2026-03-28-001
source of truth: dispatch_db
read order:
1. fetch current lane state from DB
2. fetch task payload and latest result
fallback:
- if dispatch DB unavailable, stop and report dispatch db unavailable
```

### 3.2 Database task layer

The database stores:

- task identity
- classification
- lifecycle state
- versioned task payload
- owners / assignees / participants
- dependencies
- comments and handoff notes
- artifacts
- execution runs
- verification results
- status transitions

## 4. Classification model

Do not use one giant task-type enum. It becomes unmaintainable.

Use a layered classification model with one required primary type and several orthogonal facets.

## 4.1 Primary intent category

This is the highest-level task meaning. Every task must have exactly one.

Recommended enum: `task_intent`

- `interest_question`
  - exploratory, curiosity-driven, design discussion, feasibility question
  - may close without code change
- `engineering_delivery`
  - formal development, bug fix, feature, contract, migration, test, implementation
  - usually expects code or schema change
- `language_content`
  - translation, rewriting, wording, naming, prompt writing, document polishing
  - may not require code
- `system_change`
  - workflow adjustment, governance rule, onboarding, task-system change, repo process change
  - changes how the team works rather than product behavior

This directly matches the categories you described.

## 4.2 Work domain

This answers "which part of the world does this task belong to?"

Recommended enum: `work_domain`

- `backend`
- `frontend`
- `fullstack`
- `data`
- `qa`
- `docs`
- `prompting`
- `governance`
- `embedded`
- `hardware`
- `ops`
- `product`
- `research`

## 4.3 Execution action

This answers "what does the assignee need to do?"

Recommended enum: `execution_action`

- `answer`
- `analyze`
- `design`
- `implement`
- `review`
- `verify`
- `sync`
- `migrate`
- `seed`
- `debug`
- `document`
- `translate`
- `refactor_process`

## 4.4 Change scope

This answers "what kind of output is expected?"

Recommended enum: `change_scope`

- `no_change`
- `docs_only`
- `schema_only`
- `code_only`
- `code_and_docs`
- `code_and_schema`
- `cross_repo`
- `hardware_only`
- `firmware_only`

## 4.5 Delivery lane

This answers "who should execute first?"

Recommended enum: `delivery_lane`

- `software_engineer`
- `lovable`
- `embedded_engineer`
- `hardware_engineer`
- `qa`
- `pm`
- `shared`

## 4.6 Optional tags

Use tags for finer slicing that should not become columns.

Examples:

- `seed`
- `e2e`
- `compat`
- `contract`
- `uat`
- `bootstrap`
- `encoding`
- `path-governance`
- `cloud-dev`
- `serial-debug`
- `burning-tool`

## 4.7 Why this model is better

This avoids exploding file-name categories like:

- "backend-question"
- "backend-research"
- "backend-language"
- "backend-governance"

Instead, the same task can be queried through facets:

- intent = `interest_question`
- domain = `backend`
- action = `analyze`

or:

- intent = `system_change`
- domain = `governance`
- action = `refactor_process`

## 5. Lifecycle model

Use one shared lifecycle for all tasks, but allow different close reasons per intent category.

Recommended `task_status`:

- `draft`
- `ready`
- `active`
- `waiting_input`
- `blocked`
- `waiting_sync`
- `waiting_verify`
- `in_review`
- `verified`
- `done`
- `canceled`
- `superseded`
- `archived`

Recommended transition rules:

- `draft -> ready`
- `ready -> active`
- `active -> blocked`
- `active -> waiting_input`
- `active -> waiting_sync`
- `active -> waiting_verify`
- `active -> in_review`
- `in_review -> active`
- `in_review -> verified`
- `verified -> done`
- `ready|active|blocked -> canceled`
- any non-final state -> superseded
- done/canceled/superseded -> archived

## 5.1 Close reasons

Recommended `close_reason`:

- `answered`
- `implemented`
- `verified_pass`
- `done_without_change`
- `rejected`
- `duplicate`
- `merged_into_parent`
- `out_of_scope`
- `timed_out`

This is important because not every task should end as "implemented".

Examples:

- `interest_question` can close as `answered`
- `language_content` can close as `done_without_change` or `implemented`
- `engineering_delivery` should usually close as `implemented` or `verified_pass`
- `system_change` can close as `implemented` or `done_without_change`

## 6. Multi-user collaboration model

The system must support:

- one creator
- one current owner
- multiple assignees over time
- multiple participants
- watchers / subscribers
- approvers / verifiers
- cross-lane handoff

Do not force one task = one person forever.

## 6.1 Actor types

Recommended `actor_type`:

- `human`
- `ai_agent`
- `system`

Recommended `actor_role` examples:

- `pm`
- `software_engineer`
- `frontend_engineer`
- `embedded_engineer`
- `hardware_engineer`
- `qa`
- `reviewer`
- `codex`
- `cursor`
- `lovable`

## 6.2 Team model

Use teams separately from users.

Recommended `team_code`:

- `pm`
- `backend`
- `frontend`
- `embedded`
- `hardware`
- `qa`
- `governance`

A task can be:

- owned by one team
- assigned to one user
- participated in by many users

## 6.4 Multi-project scope model

Project scope must be first-class, not just a free-text field.

Recommended scope hierarchy:

- `portfolio`
  - optional top-level grouping, for example a product line or business cluster
- `program`
  - optional grouping for a wave or transformation stream
- `project`
  - the main delivery unit
- `repo`
  - the code or document repository
- `workspace`
  - the concrete working directory or execution environment

Not every organization needs all five levels on day one, but the model should allow them.

### Recommended rules

- every task has exactly one `primary_project_id`
- a task may relate to many additional projects
- every task may also have:
  - `primary_repo_id`
  - `workspace_scope`
  - `environment_scope`
- a governance or platform task may be marked as `cross_project = true`

Examples:

- a frontend wording task
  - one project
  - one repo
- a shared region-reference data cleanup
  - one primary project
  - many related projects
  - cross-project = true
- a governance task like changing task protocol
  - project-neutral or platform-owned
  - cross-project = true

## 6.5 Multi-project ownership rules

Recommended ownership model:

- `primary_project_id`
  - the project that owns delivery and closure
- `related_project_ids`
  - other impacted projects
- `owning_team_id`
  - the team accountable for progression
- `assigned_actor_id`
  - the current executor

This avoids a common failure mode where one cross-project task has many stakeholders but no accountable owner.

## 6.6 Project membership and visibility

For multi-user collaboration, users should not see or change every task by default.

Recommended visibility dimensions:

- `private`
  - visible only to owner, assignee, PM, and admins
- `project`
  - visible to actors with membership in the primary project
- `cross_project`
  - visible to members of linked projects
- `team`
  - visible to a specific team lane
- `public_internal`
  - visible to all internal users

Recommended rule:

- task visibility should be computed from both project membership and team role

Example:

- a backend engineer in Project A should not automatically edit a hardware task in Project B
- but a PM or governance admin may still see both

## 6.3 Collaboration states

Add task-side collaboration fields:

- `owner_actor_id`
- `owner_team_id`
- `assigned_actor_id`
- `current_lane`
- `next_handoff_lane`
- `review_required`
- `verify_required`

This allows a task to move from:

- PM -> software engineer -> Lovable -> QA

without losing a unified trace.

## 7. Data model

Below is the recommended logical schema.

## 7.1 `dispatch_actor`

Represents any human or AI identity.

Key columns:

- `id`
- `actor_code`
- `actor_name`
- `actor_type`
- `actor_role`
- `team_id`
- `status`
- `external_identity_json`
- `created_at`
- `updated_at`

## 7.2 `dispatch_team`

Represents an execution lane or organizational group.

Key columns:

- `id`
- `team_code`
- `team_name`
- `status`
- `description`

## 7.2a `dispatch_project`

Represents a delivery project, not just a repo name.

Key columns:

- `id`
- `project_code`
- `project_name`
- `project_type`
- `status`
- `portfolio_id`
- `program_id`
- `default_repo_scope`
- `default_workspace_scope`
- `created_at`
- `updated_at`

Recommended `project_type`:

- `product_delivery`
- `platform`
- `governance`
- `research`
- `hardware`
- `firmware`

## 7.2b `dispatch_project_membership`

Defines which actors participate in which projects.

Key columns:

- `id`
- `project_id`
- `actor_id`
- `membership_role`
- `status`
- `joined_at`
- `left_at`

Recommended `membership_role`:

- `owner`
- `manager`
- `engineer`
- `reviewer`
- `observer`
- `qa`

## 7.2c `dispatch_repo`

Represents a repository or codebase unit.

Key columns:

- `id`
- `repo_code`
- `repo_name`
- `repo_kind`
- `canonical_path`
- `default_branch`
- `status`

Recommended `repo_kind`:

- `backend`
- `frontend`
- `embedded`
- `hardware_docs`
- `governance_docs`

## 7.3 `dispatch_task`

The canonical live task row.

Key columns:

- `id`
- `task_id`
- `title`
- `primary_project_id`
- `primary_repo_id`
- `task_intent`
- `work_domain`
- `execution_action`
- `change_scope`
- `delivery_lane`
- `status`
- `close_reason`
- `priority`
- `severity`
- `risk_level`
- `workspace_scope`
- `repo_scope`
- `branch_scope`
- `cross_project`
- `project_visibility`
- `source_channel`
- `source_reference`
- `wave_code`
- `parent_task_id`
- `owner_actor_id`
- `owner_team_id`
- `assigned_actor_id`
- `next_handoff_lane`
- `review_required`
- `verify_required`
- `created_by`
- `created_at`
- `updated_at`
- `closed_at`

Recommended notes:

- `primary_project_id` is mandatory
- `cross_project = true` means this task impacts more than one project
- `repo_scope` can remain as a string for fast display, but should not replace proper repo links

## 7.4 `dispatch_task_version`

Stores versioned payload, so every task body is traceable.

Key columns:

- `id`
- `task_pk`
- `version_no`
- `payload_md`
- `summary_md`
- `context_json`
- `change_reason`
- `created_by`
- `created_at`

Rule:

- every material task-body change creates a new version row

## 7.5 `dispatch_task_status_log`

Stores lifecycle transitions.

Key columns:

- `id`
- `task_pk`
- `from_status`
- `to_status`
- `action_code`
- `reason_code`
- `reason_text`
- `operator_actor_id`
- `snapshot_json`
- `created_at`

## 7.6 `dispatch_task_assignment_log`

Stores ownership and assignee changes.

Key columns:

- `id`
- `task_pk`
- `from_owner_actor_id`
- `to_owner_actor_id`
- `from_assigned_actor_id`
- `to_assigned_actor_id`
- `from_lane`
- `to_lane`
- `operator_actor_id`
- `remark`
- `created_at`

## 7.7 `dispatch_task_link`

Stores task relationships.

Recommended `link_type`:

- `depends_on`
- `blocks`
- `duplicates`
- `child_of`
- `related_to`
- `caused_by`
- `verifies`

Key columns:

- `id`
- `src_task_pk`
- `dst_task_pk`
- `link_type`
- `created_by`
- `created_at`

## 7.7a `dispatch_task_project_link`

Stores additional project links for cross-project tasks.

Key columns:

- `id`
- `task_pk`
- `project_id`
- `link_role`
- `created_by`
- `created_at`

Recommended `link_role`:

- `primary`
- `impacted`
- `depends_on_project`
- `shared_service_consumer`

Rule:

- one task must have exactly one `primary` project
- the same task may have many `impacted` projects

## 7.8 `dispatch_task_tag`

Stores many-to-many tags.

Key columns:

- `id`
- `task_pk`
- `tag_code`
- `tag_label`

## 7.9 `dispatch_comment`

Stores discussion and handoff notes.

Key columns:

- `id`
- `task_pk`
- `comment_type`
- `body_md`
- `visibility`
- `author_actor_id`
- `created_at`

Recommended `comment_type`:

- `note`
- `handoff`
- `question`
- `answer`
- `review`
- `risk`

## 7.10 `dispatch_artifact`

Stores outputs and references.

Key columns:

- `id`
- `task_pk`
- `artifact_type`
- `artifact_path`
- `artifact_repo`
- `artifact_commit_sha`
- `content_md`
- `checksum`
- `created_by`
- `created_at`

Recommended `artifact_type`:

- `task_file_snapshot`
- `result_file_snapshot`
- `code_patch_ref`
- `commit_ref`
- `test_report`
- `log_excerpt`
- `doc_export`

## 7.11 `dispatch_run`

Stores a concrete execution attempt by a human or AI.

Key columns:

- `id`
- `task_pk`
- `run_no`
- `runner_actor_id`
- `run_mode`
- `start_at`
- `end_at`
- `result_status`
- `verification_status`
- `commit_sha`
- `summary_md`

This is important because the same task may be attempted multiple times.

## 7.12 `dispatch_lane_current`

This replaces the old "one file is the only current state" idea with a materialized current board.

Key columns:

- `id`
- `lane_code`
- `active_task_pk`
- `work_mode`
- `execute_now_md`
- `updated_by`
- `updated_at`

This table powers the local bootstrap file.

## 8. Recommended query model

The system should support fast queries like:

- all `interest_question` tasks created this week
- all `engineering_delivery` tasks assigned to backend and still `blocked`
- all `language_content` tasks completed by Codex
- all `system_change` tasks touching governance docs
- all tasks linked to a given wave
- all active tasks in Project A
- all cross-project tasks impacting Project A and Project B
- all tasks visible to a given user across all projects
- all tasks whose latest run failed verification
- all tasks waiting for frontend sync

This is the main reason to use structured fields instead of only markdown files.

## 9. Local bootstrap to DB protocol

Recommended read protocol for AI:

1. read the local `CURRENT.md`
2. if `source of truth = dispatch_db`, fetch the lane current row
3. fetch the active task row
4. fetch the latest task version
5. fetch linked dependencies
6. fetch latest run and latest result summary
7. execute only within that scope

Recommended write protocol:

1. write a new `dispatch_run`
2. write status transition if task state changes
3. write `dispatch_artifact` or `dispatch_comment` as needed
4. optionally mirror a short human-readable summary to `RESULT.md`

## 10. Multi-user and permission model

For multi-user collaboration, permission should be lane-aware and role-aware.

For multi-project collaboration, permission must also be project-aware.

Recommended permission examples:

- PM
  - create task
  - reprioritize
  - reassign
  - close / supersede
- engineer / AI assignee
  - update run result
  - add artifact
  - move active -> blocked / waiting_verify
  - not allowed to close someone else's task without policy
- reviewer / QA
  - move in_review -> verified
  - add review comment
- system
  - update lane current
  - generate bootstrap export

Recommended project-aware permission examples:

- project member
  - view tasks in that project
  - update tasks assigned within that project according to role
- cross-project participant
  - comment and handoff, but not necessarily reprioritize
- project manager
  - reprioritize and reassign within owned projects
- portfolio / governance admin
  - view and manage cross-project tasks

Recommended rule:

- only one current owner
- many participants allowed
- approval and verification should be separate from implementation when needed
- task update rights should be checked against both team role and project membership

## 11. Recommended ID strategy

Human-readable task ids should remain.

Recommended pattern:

- `INT-2026-03-28-001` for `interest_question`
- `ENG-2026-03-28-001` for `engineering_delivery`
- `LAN-2026-03-28-001` for `language_content`
- `SYS-2026-03-28-001` for `system_change`

This makes the task intent visible at a glance without depending only on DB filters.

If you want domain-coded IDs, extend with a domain segment:

- `ENG-BE-2026-03-28-001`
- `ENG-FE-2026-03-28-001`
- `SYS-GOV-2026-03-28-001`

## 12. Recommended examples

### 12.1 Interest question

- `task_intent = interest_question`
- `work_domain = research`
- `execution_action = analyze`
- `change_scope = no_change`
- `delivery_lane = software_engineer`

Use case:

- "云端开发到底值不值得，我们怎么分层？"

### 12.2 Formal engineering

- `task_intent = engineering_delivery`
- `work_domain = backend`
- `execution_action = implement`
- `change_scope = code_and_schema`
- `delivery_lane = software_engineer`

Use case:

- "修复 runtime-order e2e，并补齐 migration / seed"

### 12.3 Language task

- `task_intent = language_content`
- `work_domain = docs`
- `execution_action = translate`
- `change_scope = docs_only`
- `delivery_lane = software_engineer`

Use case:

- "把新机器环境说明整理成清晰中文"

### 12.4 System adjustment

- `task_intent = system_change`
- `work_domain = governance`
- `execution_action = refactor_process`
- `change_scope = code_and_docs`
- `delivery_lane = pm`

Use case:

- "把任务系统从文件主导迁移到 DB 主导"

## 13. Migration path from the current file system

Do not switch in one step.

### Phase A: mirror mode

- files remain the live entry
- DB mirrors:
  - lane current
  - task metadata
  - copied markdown payload

### Phase B: hybrid live mode

- `CURRENT.md` remains as a tiny bootstrap
- DB becomes the live task truth
- `RESULT.md` becomes a summary export

### Phase C: DB-primary mode

- active work is managed in DB
- historical markdown task sheets move to archive
- files stay only for onboarding, fallback, and readable exports

## 14. Final recommendation

For this project, the recommended architecture is:

- minimal local bootstrap files
- DB-backed task system of record
- first-class project and repo entities
- layered classification by:
  - `task_intent`
  - `work_domain`
  - `execution_action`
  - `change_scope`
  - `delivery_lane`
- full versioning, assignment logs, status logs, and artifacts
- one current owner, many participants, explicit handoff
- one primary project, many related projects when needed

This will scale much better than continuing to encode task meaning only in markdown file names.
