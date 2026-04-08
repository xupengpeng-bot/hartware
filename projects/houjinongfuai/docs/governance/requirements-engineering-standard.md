# Requirements Engineering Standard

Status: active
Audience: PM, requirements engineer, AI agents
Purpose: define how fuzzy product intent becomes stable requirement truth before executable delivery tasks exist.

## Core rule

Fuzzy demand is allowed at intake.

But fuzzy demand must not directly become executable engineering work.

It must first pass through requirement analysis and requirement documentation.

## What fuzzy demand looks like

Typical examples:

- "this feature should roughly work like this"
- "I want this part to feel smarter"
- "we may need to support this direction later"
- "the execution history suggests this area needs adjustment"

These are valid inputs.
They are not yet executable task truth.

## Standard flow

1. capture the fuzzy intent
2. classify it as `INTEREST` or requirement-analysis work
3. requirements engineer reviews:
   - business requirement docs
   - product rules
   - execution history
   - UAT findings
4. requirements engineer updates or creates requirement truth in the business repository
5. if this is a requirement change, mark the global impact surface explicitly
6. PM confirms the requirement understanding or change-impact package when needed
7. PM freezes the requirement scope enough for delivery
8. executable tasks are decomposed afterwards

## Inputs for requirement analysis

Requirements analysis should use all relevant sources:

1. current business requirement docs
2. current UAT docs
3. execution history in task sheets and `RESULT.md`
4. recent verification failures
5. product-rule changes from PM

If the work is a requirement change, also analyze likely global impact points before task split.

## Outputs expected from requirements engineering

At minimum:

1. clarified business goal
2. in-scope and out-of-scope
3. object and field impact
4. global impact points
5. acceptance expectations
6. open questions
7. recommended split into later execution tasks

## Hard rule

Do not force PM to provide fully polished requirements at the first sentence.

The system must support iterative sharpening.

But do not skip the sharpening step.

## Relation to execution tasks

Requirements engineering may end with:

- requirement update only
- requirement update plus later task split recommendation

If the requirement was changed rather than newly added, the output should also include impact markers for downstream executors.

It does not automatically mean code should change in the same round.

## Confirmation loop

Use `requirement-confirmation-loop.md` and `REQUIREMENT-CONFIRMATION-TEMPLATE.md` when PM confirmation is needed before execution.
