# Protocol Source Of Truth

Version: `v1.0.0`
Date: `2026-04-11`
Status: active
Audience: embedded engineers, platform engineers, debug engineers, AI agents

## Canonical URL

The only canonical protocol reference for this board family is:

- [http://xupengpeng.top/ops/interface-protocols#device-protocols](http://xupengpeng.top/ops/interface-protocols#device-protocols)

## Working Rule

When a field, message shape, query code, action code, or feature exposure looks inconsistent:

1. check the canonical URL first
2. treat the live protocol page as higher priority than stale chat records
3. if firmware and platform disagree, align firmware to the canonical URL unless the change would be unsafe on hardware
4. if the canonical URL is incomplete for a new feature, freeze an extension note in repository docs before coding

## Current V1 Board Contract Reminder

This board currently targets the compact V1 protocol:

- top-level envelope: `v/t/i/m/s/c/r/p`
- inbound types: `SC/QR/EX`
- stable queries: `qcs/qwf/qem`
- stable actions: `spu/tpu/pas/res/ppu`
- pump supply switching is expressed by `bkr`, not `pdc`
- if `ebr` is exposed, `qem` and `SS` must carry `mp/vv/ia/pw/ek`
- if `bkf` is exposed, `HB/SS/qcs` must carry `brs`

## Safety Rule

Protocol alignment must not override safety constraints.

Examples:

- do not expose a feature in `fm` unless hardware is real and code path is implemented
- do not accept an action just because platform sends it if local protection state forbids it
- do not keep legacy compatibility branches on production images once platform has switched fully to V1

## Repository Rule

Any future protocol update must also update:

- [FIRMWARE_MODULAR_DEVELOPMENT_STANDARD_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/FIRMWARE_MODULAR_DEVELOPMENT_STANDARD_V1_0_0.md)
- [PLATFORM_DEVICE_COMMAND_CONTRACT_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PLATFORM_DEVICE_COMMAND_CONTRACT_V1_0_0.md)
- [FIRMWARE_DEVELOPMENT_DOC_INDEX_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/FIRMWARE_DEVELOPMENT_DOC_INDEX_V1_0_0.md)
