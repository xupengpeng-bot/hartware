# Marketing Strategy Standard

Status: active
Audience: PM, marketing strategy engineer, AI agents
Purpose: turn fuzzy marketing intent into high-quality, audience-aware output without drifting from actual product truth.

## Core rule

Marketing requests are allowed to be fuzzy.

But the output must still be grounded in:

1. project reality
2. current delivery status
3. target audience
4. proof-backed advantages

## Typical fuzzy inputs

Examples:

- "help me shape a marketing plan"
- "how should we introduce this project externally"
- "give me a sellable set of core talking points"
- "prepare one version of material for a specific audience"

These are valid starting points.

## Required reading order for marketing work

1. `<BUSINESS_REPO_ROOT>\docs\README.md`
2. `<BUSINESS_REPO_ROOT>\docs\requirements\README.md`
3. the business system-docs README under `<BUSINESS_REPO_ROOT>\docs`
4. the relevant business overview / overall requirement / requirement-breakdown docs linked from the system-docs README
5. `PROJECT-MARKETING-BRIEF.md`
6. relevant execution history or current milestone notes when the material depends on delivery status

## Standard output structure

High-quality marketing output should include at least:

1. target audience
2. audience pain points
3. project advantages to emphasize
4. proof points or evidence anchors
5. objections and response direction
6. recommended material format

## Audience split rule

Do not produce one generic message for everyone if the audience matters.

At minimum, choose which audience you are writing for:

- project operator
- operations and maintenance
- finance or investor
- government or platform owner
- partner or reseller

## Quality rule

Prefer:

- clear value proposition
- differentiated advantages
- realistic proof points
- audience-specific wording

Avoid:

- generic AI-style slogans
- unsupported claims
- product features with no business meaning
- pretending unfinished capability is already fully delivered

## Relation to role model

Default owner role for this kind of work:

- `marketing_strategy_engineer`

Default task type:

- `LANGUAGE` when polishing wording only
- `INTEREST` when exploring messaging directions
- `SYSTEM` when building the marketing workflow or standards
