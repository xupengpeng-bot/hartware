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

- "这个功能大概这样做"
- "我想把这里做得更智能一点"
- "后面可能要支持这个方向"
- "我看执行历史感觉这里要调整"

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
5. PM freezes the requirement scope enough for delivery
6. executable tasks are decomposed afterwards

## Inputs for requirement analysis

Requirements analysis should use all relevant sources:

1. current business requirement docs
2. current UAT docs
3. execution history in task sheets and `RESULT.md`
4. recent verification failures
5. product-rule changes from PM

## Outputs expected from requirements engineering

At minimum:

1. clarified business goal
2. in-scope and out-of-scope
3. object and field impact
4. acceptance expectations
5. open questions
6. recommended split into later execution tasks

## Hard rule

Do not force PM to provide fully polished requirements at the first sentence.

The system must support iterative sharpening.

But do not skip the sharpening step.

## Relation to execution tasks

Requirements engineering may end with:

- requirement update only
- requirement update plus later task split recommendation

It does not automatically mean code should change in the same round.
