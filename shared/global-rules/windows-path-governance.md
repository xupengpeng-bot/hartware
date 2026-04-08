# Windows Path Governance

Status: active
Audience: PM, engineers, AI agents
Purpose: reduce Windows-specific rework caused by path-length limits, deep nesting, and toolchain path fragility.

## Why this exists

Windows path issues create expensive non-business rework:

- clone or checkout failures
- toolchains that work on one machine but fail on another
- long nested task filenames that are readable but operationally fragile
- `node_modules` or generated files pushing a workable tree over the edge

## Current strategy

1. Keep the workspace root short.
   - use a shallow root such as `D:\20251211\zhinengti`
2. Split business code and development-system workspaces at the sibling level instead of nesting them deeper.
3. Prefer repository-relative paths in docs and task packages.
4. Avoid unnecessary multi-level folder nesting for templates, history, and handoff materials.
5. Keep task file names expressive but not essay-length.

## System and Git requirements

On Windows machines used for this workflow:

1. system long-path support should be enabled
   - `LongPathsEnabled = 1`
2. Git long-path support should be enabled for active repositories
   - `git config core.longpaths true`

If either is missing, treat that as an environment-risk item.

## Project naming rule

When creating files, prefer:

- shorter folder depth
- shorter task slugs
- stable abbreviations where the meaning remains clear

Avoid:

- deeply nested archive trees for active work
- repeated long prefixes inside file names
- machine-local directories with unnecessary Chinese and English duplication in the same path

## Preflight rule

Preflight should warn when:

- Git long-path support is not enabled
- the repository max tracked file path is approaching a risky threshold

This is an efficiency guardrail, not just a compatibility note.
