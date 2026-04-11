# Firmware Development Doc Index

Version: `v1.0.0`
Date: `2026-04-11`
Status: active

## Purpose

This index points to the current reusable development baseline for controller firmware projects.

## Documents

### 1. Architecture and development rules

- [FIRMWARE_MODULAR_DEVELOPMENT_STANDARD_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/FIRMWARE_MODULAR_DEVELOPMENT_STANDARD_V1_0_0.md)

Use this when:

- defining a new board architecture
- deciding where a new capability should be implemented
- deciding whether a feature is ready to expose

### 2. Platform wire contract

- [PLATFORM_DEVICE_COMMAND_CONTRACT_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PLATFORM_DEVICE_COMMAND_CONTRACT_V1_0_0.md)

Use this when:

- platform asks what it can send
- platform asks what fields are allowed on wire
- platform asks how to request a new action or query

### 3. Protocol source of truth

- [PROTOCOL_SOURCE_OF_TRUTH_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PROTOCOL_SOURCE_OF_TRUTH_V1_0_0.md)

Use this when:

- protocol fields look inconsistent across page, code, and platform UI
- firmware needs to verify the released wire contract
- someone asks which protocol page is authoritative

### 4. Debug handoff checklist

- [DEBUG_HANDOFF_CHECKLIST_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/DEBUG_HANDOFF_CHECKLIST_V1_0_0.md)

Use this when:

- debug engineers are bringing up new RS485 devices
- hardware team asks what logs or protocol material firmware needs
- acceptance gets blocked by missing field information

### 5. Current board accepted command list

- [PLATFORM_ACCEPTED_COMMANDS_20260411.txt](/D:/20251211/智能体/hardware/new/PLATFORM_ACCEPTED_COMMANDS_20260411.txt)

Use this when:

- platform needs the exact currently released command set
- someone wants to know what the current board truly accepts right now

### 6. Production minimal build checklist

- [PRODUCTION_MINIMAL_BUILD_CHECKLIST_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PRODUCTION_MINIMAL_BUILD_CHECKLIST_V1_0_0.md)

Use this when:

- preparing a production build
- trimming non-current-board modules from the image
- checking whether wire protocol and released capability set are minimal

### 7. Firmware experience library

- [FIRMWARE_EXPERIENCE_LIBRARY_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/FIRMWARE_EXPERIENCE_LIBRARY_V1_0_0.md)
- [FIRMWARE_EXPERIENCE_ENTRY_TEMPLATE_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/FIRMWARE_EXPERIENCE_ENTRY_TEMPLATE_V1_0_0.md)

Use this when:

- a previous engineering conclusion is corrected by stronger evidence
- you want reusable lessons from debugging, protocol work, safety behavior, or performance tuning
- future boards should avoid repeating the same mistake

### 8. Meter epoch and counter reset contract

- [METER_EPOCH_COUNTER_RESET_CONTRACT_20260411.md](/D:/20251211/智能体/hardware/new/code/firmware/METER_EPOCH_COUNTER_RESET_CONTRACT_20260411.md)

Use this when:

- platform asks how to detect meter discontinuity
- firmware needs the released short-code contract for `meter_epoch`
- billing alignment depends on reboot, counter clear, meter replacement, or abnormal recovery facts

## Recommended sharing rule

Send these documents by audience:

- platform team:
  - command contract
  - protocol source of truth
  - accepted command list
- debug team:
  - protocol source of truth
  - debug handoff checklist
- firmware team:
  - modular development standard
  - protocol source of truth
  - command contract
  - debug handoff checklist
  - production minimal build checklist
  - firmware experience library

## Current baseline reminder

Current board released capability set:

- `pay`
- `cdr`
- `ebr`
- `bkr`
