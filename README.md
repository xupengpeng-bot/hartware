# Hartware Workspace

Status: active  
Audience: firmware, hardware, debug, AI agents

This repository now serves two roles at once:

- active embedded source/build workspace
- development-system workspace for embedded delivery and governance

## Current Board Identity

The active board in the current firmware is:

- hardware code: `HW-SCAN-IRR-CTRL-4G-A01`
- hardware sku: `SCAN-IRR-CTRL-4G`
- hardware revision: `A01`
- software family: `SCAN-IRRIGATION-CONTROL`

Source of truth:

- [scan_trial_defs.h](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/config/scan_trial_defs.h:10)

## Structure

- `code`
  - active shared firmware source tree
- `build_flash`
  - active flashing, serial capture, and local bench helpers used by the current build chain
- `build_codex_arm`
  - current local ARM build output
- `build_codex_check`
  - local validation/build check output
- `products`
  - per-product firmware baselines copied from the active shared program when a new product line needs to branch independently
- `Hardware`
  - board-specific hardware assets grouped by hardware name
- `projects`
  - project-specific development-system workspace
- `shared`
  - shared development-system rules and templates

## Hardware Layout Rule

For multi-product work, create one hardware directory per board under `Hardware/` using the hardware identity:

- recommended pattern: `Hardware/<HARDWARE-SKU>-<REV>`
- current board directory: [Hardware/SCAN-IRR-CTRL-4G-A01](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/Hardware/SCAN-IRR-CTRL-4G-A01)

Put these board-specific assets there:

- schematics
- board photos and pinout screenshots
- board-specific flashing bundles or historical tools
- hardware-only notes

Keep these shared at repository root unless we do a dedicated build-system split:

- `code`
- `build_flash`
- active local build output directories

This keeps the current build scripts working while making room for more board variants.

## Product Baselines

- [products/SCAN-IRR-VALVE-CTRL-4G-A01](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01)
  - valve-control product baseline copied from the current active controller program on 2026-04-14
  - contains its own `code` and `build_flash` so follow-up edits can stay isolated from the current root mainline
  - this is the default working directory for `阀控 01`; if the user asks to adjust valve-control firmware, start here unless the task is explicitly a shared root-program change

## Current Project Folder

- [projects/houjinongfuai](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/projects/houjinongfuai)
  - development-system workspace for the `houjinongfuai` project

## Root Workspace Map

- [ROOT-WORKSPACE-MAP.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/ROOT-WORKSPACE-MAP.md)
  - root-level workspace classification for formal, sidecar, and temporary directories

## Shared Rules

- `shared/global-rules`
  - shared task classification
  - Windows-first encoding governance
  - development-system evolution rules

## Separation Rule

- embedded business code and reusable firmware stay in `code`
- product-specific source copies that intentionally diverge go under `products/<board>`
- per-board hardware material goes under `Hardware/<board>`
- project delivery/governance assets stay in `projects` and `shared`
