# Requirement Confirmation Loop

Status: active
Audience: PM, delivery_orchestrator, requirements_engineer, software_engineer, uat_engineer
Purpose: keep PM involved at the highest-value confirmation points without requiring PM to manually manage every downstream step.

## Core rule

PM does not need to micromanage every stage.

But PM should confirm at the points where misunderstanding would be expensive.

## Confirmation checkpoints

### Checkpoint 1: requirement understanding confirmation

Owner:

- `requirements_engineer`
- confirmed by PM

Required confirmation package:

1. clarified goal
2. scope
3. non-goals
4. acceptance logic
5. open questions

This checkpoint confirms:

- the system understood the requirement correctly

### Checkpoint 2: requirement-change impact confirmation

Owner:

- `requirements_engineer`
- `delivery_orchestrator`
- confirmed by PM when the change is meaningful

Required confirmation package:

1. change summary
2. reason for change
3. global impact points
4. non-impacted areas
5. required follow-up roles
6. acceptance impact

This checkpoint confirms:

- the change does not silently introduce unseen follow-on work

### Checkpoint 3: execution-slice confirmation

Owner:

- `delivery_orchestrator`
- confirmed by PM only if the decomposition changed risk, scope, or sequence materially

Required package:

1. role sequence
2. execution slices
3. verification method
4. handoff targets

This checkpoint confirms:

- the system is solving the right problem in the right order

## Default policy

### New requirement

Recommended PM confirmations:

1. checkpoint 1
2. checkpoint 3 when the decomposition is not obvious

### Requirement change

Recommended PM confirmations:

1. checkpoint 2
2. checkpoint 3 if the impact touches multiple roles or domains

## Anti-friction rule

Do not ask PM for confirmation after every tiny task split.

Ask only when one of these is true:

- business meaning changed
- acceptance logic changed
- global impact widened
- role sequence materially changed
- risk increased

## Execution rule

If PM already confirmed the requirement understanding package and the later task split stays within that confirmed frame, downstream execution may continue without repeating the same confirmation.
