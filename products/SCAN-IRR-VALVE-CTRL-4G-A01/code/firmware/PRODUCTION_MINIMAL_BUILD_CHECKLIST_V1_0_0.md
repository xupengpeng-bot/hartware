# Production Minimal Build Checklist

Version: `v1.0.0`
Date: `2026-04-11`
Status: active
Audience: firmware engineers, platform engineers, debug engineers

## Canonical Protocol Source

The only canonical protocol reference for this board family is:

- [http://xupengpeng.top/ops/interface-protocols#device-protocols](http://xupengpeng.top/ops/interface-protocols#device-protocols)

## Current Board Production Goal

This board is treated as a pump supply switch controller.

Pump supply switching must be exposed as `bkr`.

Do not use `pdc` to describe breaker close or breaker open for this board.

## Current Board Released Feature Surface

Expose only real released features:

- `pay` only if QR payment control is real and enabled
- `cdr` only if card reader hardware is real and enabled
- `ebr` only if electric meter read is real and enabled
- `bkr` only if pump supply switch execute path is real and enabled
- `bkf` only if breaker feedback is real and enabled
- `svl` only on valve variants with real valve hardware and released valve control

Do not expose these on the current production image unless a future board variant truly needs them:

- `pdc`
- `pwm`
- `prs`
- `flw`
- `sma`
- `sta`
- `rse`
- `ale`
- `alp`
- `ahp`

## Pump Control Mode Rules

Pump supply control mode must be explicit in `cc.pump_control_mode`.

Allowed released values:

- `meter_breaker_485`
- `relay_direct`
- `contactor_direct`

Required wire behavior by mode:

### meter_breaker_485

- `fm` must include at least `ebr` and `bkr`
- add `bkf` only if breaker feedback is real
- `mp` must be the real meter protocol, for example `dlt645_2007`
- `cp` must be the real control protocol

### relay_direct

- `fm` must include at least `bkr`
- add `bkf` only if breaker feedback is real
- `cp` must be `relay_direct`

### contactor_direct

- `fm` must include at least `bkr`
- add `bkf` only if breaker feedback is real
- `cp` must be `contactor_direct`

## Production Wire Contract

Keep only the compact protocol:

- top level envelope: `v/t/i/m/s/c/r/p`
- inbound types: `SC/QR/EX`
- `EX.p` keys: `sc/ac/tr/pm`
- `QR.p` keys: `sc/qc/tr/pm`
- `SC.p` keys: `cv/fm/rr/pc/cc`
- `ER.p` keys: `ec/rc/msg/tr`

Rules:

- do not keep long field compatibility on production images
- do not send `null`
- do not send empty string, empty array, or empty object
- omit optional field when value is absent
- unsupported or invalid request must return `NK`

## Current Released Actions

- `sc=md,ac=spu,tr=pump_1`
- `sc=md,ac=tpu,tr=pump_1`
- `sc=wf,ac=pas`
- `sc=wf,ac=res`
- `sc=cm,ac=ppu`

Valve actions are not part of the current production profile unless a real valve variant is released:

- `sc=md,ac=ovl,tr=valve_1`
- `sc=md,ac=cvl,tr=valve_1`

## Current Released Queries

- `sc=cm,qc=qcs`
- `sc=wf,qc=qwf`
- `sc=cm,qc=qem`

## Required Telemetry Surface

### RG

- `hs`
- `hr`
- `ff`
- `fv`
- `cv`
- `fm`
- `mp` when `ebr` is exposed
- `cp` when `bkr` is exposed

### HB

- `rd`
- `on`
- `tc`
- `wf`
- `csq`
- `bs`
- `bv`
- `sv`
- `cv`
- `pm`
- `brs` only when `bkf` is exposed

### SS

- `wf`
- `rt`
- `mp` when `ebr` is exposed
- `vv`
- `ia`
- `pw`
- `ek`
- `brs` only when `bkf` is exposed
- `ch`

### QS:qcs

- `on`
- `rd`
- `tc`
- `wf`
- `csq`
- `bs`
- `bv`
- `sv`
- `cv`
- `pm`
- `brs` only when `bkf` is exposed

### QS:qem

- `mp`
- `vv`
- `ia`
- `pw`
- `ek`

## Current Board Modules Removed From Production Build

- `module_single_valve`
- `module_pressure`
- `module_soil_moisture`
- `module_soil_temperature`

## Current Board Default Safety Policy

- keep overload protection
- keep phase loss protection
- keep under-voltage protection
- keep over-voltage protection
- disable dry-run protection by default on this no-sensor board
- keep pressure high and pressure low defaults at `0`

## Production Acceptance Checklist

- `RG.fm` contains only real released feature codes
- `RG` uses `bkr` for pump supply switching, never `pdc`
- `RG` includes `mp/cp` only when those capabilities are real
- `HB/SS/QS` do not contain `null`
- `wf` only uses compact public codes
- `HB/SS/QS:qcs` include `brs` only when `bkf` is exposed
- `SS/QS:qem` include `mp/vv/ia/pw/ek` when `ebr` is exposed
- extra payload keys in `EX/QR/SC` return `NK rc=PI`
- unsupported actions and queries return `NK`
- build passes and artifact size stays within release budget

## Platform Pre-Release Checks

- platform sends only compact `wire_message`
- platform does not leak internal keys like `mc`, `start_token`, `action_code`, `query_code`, or `module_code`
- platform opens UI entry only from released `fm`
- platform treats breaker close and breaker open as `spu` and `tpu`
- platform does not map this board to `pdc`
