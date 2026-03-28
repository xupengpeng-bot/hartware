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

- "帮我想一个营销方案"
- "看看怎么对外介绍这个项目"
- "给商务一套可讲的卖点"
- "针对某类客户整理一版材料"

These are valid starting points.

## Required reading order for marketing work

1. `D:\20251211\zhinengti\houjinongfuai\docs\README.md`
2. `D:\20251211\zhinengti\houjinongfuai\docs\系统说明\系统业务总览简版.md`
3. `D:\20251211\zhinengti\houjinongfuai\docs\系统说明\系统整体业务需求.md`
4. `D:\20251211\zhinengti\houjinongfuai\docs\系统说明\需求拆解.md`
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
