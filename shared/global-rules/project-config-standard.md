# Project Config Standard

Status: active
Audience: PM, engineers, AI agents
Purpose: make each project keep one stable, searchable config registry so troubleshooting does not depend on tribal memory.

## Core rule

Each active project in the development-system workspace must maintain one project-level config registry:

- `projects/<project>/PROJECT-CONFIG-REGISTRY.md`

This file is the first stop for environment and configuration troubleshooting.

## Why this exists

Configuration issues are expensive because they are often scattered across:

- `.env.example`
- local `.env`
- startup scripts
- frontend env files
- DB defaults
- ports
- optional sidecar services
- machine-local toolchain settings

Without one registry, people waste time searching many files before even knowing where the real config surface is.

## Required sections

Each project config registry should include at least:

1. workspace anchors
2. active repositories
3. env files and who owns them
4. runtime ports
5. database defaults
6. optional sidecar or mirror services
7. startup scripts
8. verification entry commands
9. secret-handling rule
10. troubleshooting order

## Hard rules

- never store real secrets in the config registry
- list config keys and ownership, not secret values
- when a config source changes, update the registry in the same round if it affects active delivery
- if the registry is stale, treat that as a development-system quality issue

## AI rule

When the task involves setup, verification, startup failure, port conflict, DB mismatch, or env debugging:

1. read `PROJECT-CONFIG-REGISTRY.md`
2. then read the referenced `.env.example`, startup script, or config file

Do not start with blind guessing.
