# Firmware Experience Library

Version: `v1.0.0`
Date: `2026-04-11`
Status: active
Audience: embedded engineers, AI agents, debug engineers, platform engineers
Scope: reusable engineering experience for this board family and future derivative boards

## 1. Purpose

This repository keeps an explicit engineering experience library.

The goal is not only to store what worked, but also to store:

- what failed
- what was misunderstood at first
- what later evidence corrected
- what rule should be followed next time

This library is the visible and maintainable form of engineering memory.

## 2. Core Rule

Experience must be updated as evidence changes.

If a previous conclusion is later found to be wrong:

- do not silently delete the old conclusion
- mark the old conclusion as `superseded`
- add the new conclusion as a new entry
- link the new entry to the old one
- record the evidence that caused the correction

The fact that "an old experience may later be wrong" is itself a standing experience rule.

## 3. Status Model

Every experience entry must use one of these statuses:

- `active`
- `trial`
- `superseded`
- `retired`

## 4. Required Entry Format

Every new experience entry should contain:

- `id`
- `title`
- `status`
- `date`
- `scope`
- `problem`
- `wrong_assumption`
- `evidence`
- `current_rule`
- `implementation_note`
- `verification`
- `supersedes`
- `superseded_by`

## 5. Update Rules

Update the experience library when any of these happens:

- protocol page and firmware behavior disagree
- platform sends legal wire data but device rejects it unexpectedly
- a debugging conclusion is later overturned by logs or hardware proof
- a board capability is redefined
- an optimization breaks safety or recovery behavior
- a hidden compatibility branch causes confusion
- a module ownership boundary proves wrong
- a framing issue is finally understood

## 6. Source Priority

Experience entries must cite real evidence from at least one of:

- canonical protocol URL
- repository code
- build logs
- serial logs
- hardware protocol documents
- controlled reproduction steps

Chat alone is not enough evidence.

## 7. Current Active Experiences

### EXP-0001

- `id`: `EXP-0001`
- `title`: `Protocol source must be checked against the live canonical page`
- `status`: `superseded`
- `date`: `2026-04-11`
- `scope`: `protocol governance`
- `problem`: protocol fields and examples drift between chat, platform code, and firmware code
- `wrong_assumption`: local memory or old chat is reliable enough for protocol decisions
- `evidence`:
  - [PROTOCOL_SOURCE_OF_TRUTH_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PROTOCOL_SOURCE_OF_TRUTH_V1_0_0.md)
  - [device-protocols](http://xupengpeng.top/ops/interface-protocols#device-protocols)
- `current_rule`: when fields, actions, queries, or feature exposure look inconsistent, check the canonical page first
- `implementation_note`: production firmware should align to the canonical page unless the requested change is unsafe on hardware
- `verification`: compare firmware `RG/HB/SS/EX/QR` against the canonical page before release
- `supersedes`: `none`
- `superseded_by`: `EXP-0014`

### EXP-0002

- `id`: `EXP-0002`
- `title`: `Pump supply switching for this board must be exposed as bkr, not pdc`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `capability model`
- `problem`: earlier variants mixed pump direct control semantics with breaker close and breaker open
- `wrong_assumption`: `pdc` can safely represent pump power switching on this board
- `evidence`:
  - [PRODUCTION_MINIMAL_BUILD_CHECKLIST_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PRODUCTION_MINIMAL_BUILD_CHECKLIST_V1_0_0.md)
- `current_rule`: expose pump supply switching as `bkr`; add `bkf` only when breaker feedback is real
- `implementation_note`: `spu/tpu` are the released close and open actions for `pump_1`
- `verification`: `RG.fm` should show `bkr` and should not use `pdc` for this board profile
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0003

- `id`: `EXP-0003`
- `title`: `Optional wire fields must be omitted when absent`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `serialization`
- `problem`: empty values waste bytes and create noisy telemetry
- `wrong_assumption`: sending `null`, empty arrays, or empty objects is harmless
- `evidence`:
  - compact V1 protocol and current serializer cleanup work
- `current_rule`: if a value is absent, omit the field instead of sending `null`, `\"\"`, `[]`, or `{}`
- `implementation_note`: this especially applies to `HB`, `SS`, `QS`, `ER`, and command payloads
- `verification`: serialized wire payload should contain only required fields and present optional fields
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0004

- `id`: `EXP-0004`
- `title`: `QIRD pending length must be based on actual returned data, not requested read length`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `4G TCP receive path`
- `problem`: valid downlink commands were received but later rejected as `invalid json`
- `wrong_assumption`: if `AT+QIRD` is requested for `N` bytes and only `M` bytes are read, the remaining bytes are always `N-M`
- `evidence`:
  - log contained valid command `cmd-0747b938-3d62-4780-a578-6fefd8768ca2`
  - device later returned `NK rc=PI msg=invalid json`
  - fix was implemented in [net_4g_modem.c](/D:/20251211/智能体/hardware/new/code/firmware/net/net_4g_modem.c)
- `current_rule`: pending receive bytes must be computed from the real `QIRD` response data length and unread length, not from the request length
- `implementation_note`: keep explicit bookkeeping for `read_len`, `data_len`, and `unread_len`
- `verification`: compact `EX` commands should be accepted or rejected by business logic, but not by `invalid json`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0005

- `id`: `EXP-0005`
- `title`: `No-sensor boards must not inherit sensor-driven protection defaults`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `safety defaults`
- `problem`: a board without real pressure or flow sensors can self-trigger protections immediately after start
- `wrong_assumption`: generic pressure and dry-run defaults are safe even when sensor capability is not present
- `evidence`:
  - [config_store.c](/D:/20251211/智能体/hardware/new/code/firmware/storage/config_store.c)
- `current_rule`: when a board has no released sensor capability, disable sensor-driven protections by default unless real values are wired and verified
- `implementation_note`: keep electrical protections, disable dry-run and pressure threshold defaults
- `verification`: a start command should not fault immediately because of missing sensor inputs
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0006

- `id`: `EXP-0006`
- `title`: `Pump power loss judgment must follow the real control mode, not a single meter-power rule`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `power domain modeling`
- `problem`: a relay-direct pump start succeeded, but the session faulted immediately with `pump_power_loss`
- `wrong_assumption`: if the pump is running, `meter.power_kw <= 0.05` always means pump power is lost
- `evidence`:
  - [gui_serial_COM4_20260411_174353.txt](/D:/20251211/智能体/hardware/new/build_flash/logs/gui_serial_COM4_20260411_174353.txt)
  - [app_scheduler.c](/D:/20251211/智能体/hardware/new/code/firmware/app/app_scheduler.c)
- `current_rule`: evaluate `pump_power_domain` according to `control_config.pump_control_mode`; relay-direct and contactor-direct boards must not be faulted only because meter power is low or delayed
- `implementation_note`: for `relay_direct` and `contactor_direct`, use the confirmed pump actuator state; for `meter_breaker_485`, allow an `UNKNOWN` grace window before treating low meter power as loss
- `verification`: `EX spu` on a relay-direct board should return `AK` and remain in `RN` instead of transitioning immediately to `ER/pump_power_loss`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0007

- `id`: `EXP-0007`
- `title`: `Meter source must be reported independently from flow source on mixed-source boards`
- `status`: `superseded`
- `date`: `2026-04-11`
- `scope`: `telemetry semantics`
- `problem`: logs labeled meter snapshots as `virtual` even after DLT645 address discovery and valid meter readings
- `wrong_assumption`: if flow acquisition is virtual, the whole measurement source should be labeled virtual
- `evidence`:
  - [gui_serial_COM4_20260411_174353.txt](/D:/20251211/智能体/hardware/new/build_flash/logs/gui_serial_COM4_20260411_174353.txt)
  - [module_meter.c](/D:/20251211/智能体/hardware/new/code/firmware/modules/module_meter.c)
  - [module_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/modules/module_flow.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
- `current_rule`: meter provenance and flow provenance must be judged independently; this rule was later tightened because virtual fallbacks were removed from the default product profile
- `implementation_note`: this entry is kept as history; do not reintroduce virtual flow fallback just to preserve old log labels
- `verification`: historical only
- `supersedes`: `none`
- `superseded_by`: `EXP-0008`

### EXP-0008

- `id`: `EXP-0008`
- `title`: `Production firmware must not synthesize virtual sensor or flow data unless explicitly requested`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `product default behavior`
- `problem`: virtual fallbacks hid the line between real hardware capability and placeholder behavior, which made field diagnosis and platform interpretation harder
- `wrong_assumption`: virtual measurements are acceptable as a default development convenience in production-oriented firmware
- `evidence`:
  - user direction on 2026-04-11 to stop using virtual behavior by default
  - [module_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/modules/module_flow.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
- `current_rule`: unless the user explicitly requests a simulation mode, firmware must expose only real hardware data; unavailable sources must be reported as `unknown` or invalid, not synthesized as fake values
- `implementation_note`: remove or disable virtual defaults for flow, sensor, and derived telemetry paths in the production profile
- `verification`: logs and summaries must not contain `source=virtual`; missing real hardware should surface as invalid quality, unknown source, or omitted optional fields
- `supersedes`: `EXP-0007`
- `superseded_by`: `none`

### EXP-0009

- `id`: `EXP-0009`
- `title`: `A flashed image must be proven by runtime version before blaming stale code, and malformed downlinks must be attributed to the sender unless framing evidence says otherwise`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `field diagnosis`
- `problem`: field logs appeared to contradict the current source tree, which initially looked like an old image was still running
- `wrong_assumption`: if behavior looks old, the device must still be running an older firmware image
- `evidence`:
  - [gui_serial_COM4_20260411_180823.txt](/D:/20251211/智能体/hardware/new/build_flash/logs/gui_serial_COM4_20260411_180823.txt)
  - [flash.cmd](/D:/20251211/智能体/hardware/new/build_flash/flash.cmd)
  - [flash_standalone.cmd](/D:/20251211/智能体/hardware/new/build_flash/flash_standalone.cmd)
  - [scan_trial_defs.h](/D:/20251211/智能体/hardware/new/code/firmware/config/scan_trial_defs.h)
- `current_rule`: first prove the running image by checking runtime version fields such as boot logs and `RG.fv`; if runtime version matches the current build and the downlink is still rejected as `invalid json`, treat the downlink payload as malformed unless receive-framing evidence shows truncation or mixed frames
- `implementation_note`: keep `fv` easy to spot in register logs, and preserve enough RX logging to compare the incoming `m/c/s` fields when platform and device disagree
- `verification`: a log showing `fv` equal to the current build version proves the latest image is running; a rejected frame whose visible identifiers are internally inconsistent should be escalated to the platform side as a malformed wire message
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0010

- `id`: `EXP-0010`
- `title`: `Do not hard-restrict non-configurable sensor and capability items in the device protocol`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `protocol/config governance`
- `problem`: the firmware rejected platform sync or command payloads simply because some capability or sensor-related fields were not yet fully exposed as configurable items
- `wrong_assumption`: if the current platform UI or contract only exposes part of the model, the firmware should hard-reject any extra known capability codes or extra payload keys
- `evidence`:
  - [proto_execute_action.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_execute_action.c)
  - [proto_query.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_query.c)
  - [proto_sync_config.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_sync_config.c)
  - [proto_register.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_register.c)
- `current_rule`: when a sensor, flow, or capability item is not yet fully configurable from the platform, do not block the device with narrow payload white-lists or a tiny `fm` allow-list; keep required structure checks, accept known aliases, ignore unknown optional items, and let active config drive reporting
- `implementation_note`: keep mandatory message shape validation, but remove rigid root/payload key white-lists for `EX/QR/SC`; expand `fm` parsing/reporting to all known short capability codes already supported by the firmware model
- `verification`: platform payloads with extra non-critical fields no longer fail at the protocol layer, and `RG.fm` reflects the broader active capability set instead of a fixed six-item subset
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0011

- `id`: `EXP-0011`
- `title`: `A control path is not real until it drives hardware and avoids pin conflicts`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `actuator implementation`
- `problem`: the device logged `START_SUCCESS`, entered `RN`, and returned `AK`, but the pump relay did not move on the real board
- `wrong_assumption`: if the state machine, telemetry, and protocol reply all show success, the board must have executed the action physically
- `evidence`:
  - [module_pump_vfd.c](/D:/20251211/智能体/hardware/new/code/firmware/modules/module_pump_vfd.c)
  - [bsp_gpio.c](/D:/20251211/智能体/hardware/new/code/firmware/bsp/bsp_gpio.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
  - [board_hw_config.h](/D:/20251211/智能体/hardware/new/code/firmware/config/board_hw_config.h)
  - [bsp_status_led.c](/D:/20251211/智能体/hardware/new/code/firmware/bsp/bsp_status_led.c)
- `current_rule`: any released actuator capability must terminate in a real board-level output implementation; if there is no hardware-backed execution path, return failure instead of mutating only software state
- `implementation_note`: verify three layers together: protocol reply, module execution, and final GPIO or bus operation; also check that no status LED or debug helper reuses the same pin as the real actuator output
- `verification`: for `relay_direct`, a successful `spu` must change the configured relay output level, and any conflicting status-indicator use of that pin must be disabled
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0012

- `id`: `EXP-0012`
- `title`: `Board capability defaults must match the real actuator transport, not the easiest local fallback`
- `status`: `superseded`
- `date`: `2026-04-11`
- `scope`: `board profile and actuator routing`
- `problem`: a board that is really controlled by meter-side breaker commands was still registered and executed as `relay_direct`, which made logs, capabilities, and field expectations diverge from the actual product variant
- `wrong_assumption`: if a local relay path exists in firmware, it is acceptable to keep the board defaults on `relay_direct` until the real meter-control path is finished
- `evidence`:
  - [config_store.c](/D:/20251211/智能体/hardware/new/code/firmware/storage/config_store.c)
  - [proto_register.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_register.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
  - [module_meter.c](/D:/20251211/智能体/hardware/new/code/firmware/modules/module_meter.c)
- `current_rule`: this entry captured a temporary assumption that the active board profile had already switched to `meter_breaker_485`; that assumption was later corrected by field confirmation
- `implementation_note`: keep this entry as history so the wrong board-profile inference is visible and traceable
- `verification`: historical only
- `supersedes`: `none`
- `superseded_by`: `EXP-0013`

### EXP-0013

- `id`: `EXP-0013`
- `title`: `Board control transport must be proven from the released profile or explicit field confirmation before changing capability defaults`
- `status`: `superseded`
- `date`: `2026-04-11`
- `scope`: `board profile governance`
- `problem`: the control profile was switched to `meter_breaker_485` based on an intermediate assumption, but the actual released board variant was still `relay_direct`
- `wrong_assumption`: a future or target architecture can be treated as the active shipped board profile before field confirmation and released wiring/profile data agree
- `evidence`:
  - user clarification on `2026-04-11` that the active board is `cp=relay_direct`, `cc.pump_control_mode=relay_direct`
  - [config_store.c](/D:/20251211/智能体/hardware/new/code/firmware/storage/config_store.c)
  - [proto_register.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_register.c)
  - [module_pump_vfd.c](/D:/20251211/智能体/hardware/new/code/firmware/modules/module_pump_vfd.c)
- `current_rule`: do not switch default control transport, `cp`, or channel `io_kind` based on inference; keep the shipped board profile on the confirmed actuator path until release wiring, platform contract, and field confirmation all align
- `implementation_note`: generic action dispatch may support multiple control modes, but the default config and reported capability must stay on the actually released board mode; if the platform later switches the board to `meter_breaker_485`, then update defaults and execution together in one change
- `verification`: default register payload reports `cp=relay_direct`, `cc.pump_control_mode=relay_direct`, and `pump_1` remains bound to the local relay path unless an explicit config update changes it
- `supersedes`: `EXP-0012`
- `superseded_by`: `EXP-0014`

### EXP-0014

- `id`: `EXP-0014`
- `title`: `Current-version board mode must follow the latest explicit product clarification until code and telemetry are fully realigned`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `board profile governance`
- `problem`: recent logs and defaults showed `relay_direct`, but the product owner later clarified that the current version should be treated as meter-controlled for breaker open and close
- `wrong_assumption`: the latest successful field log is always a stronger authority than the latest explicit product clarification
- `evidence`:
  - user clarification on `2026-04-11`: `当前版本是电表控制开合闸`
  - [config_store.c](/D:/20251211/鏅鸿兘浣?hardware/new/code/firmware/storage/config_store.c)
  - [safety_flow.c](/D:/20251211/鏅鸿兘浣?hardware/new/code/firmware/safety/safety_flow.c)
  - [module_meter.c](/D:/20251211/鏅鸿兘浣?hardware/new/code/firmware/modules/module_meter.c)
- `current_rule`: until explicitly changed again, treat the current-version product requirement as meter-controlled for breaker open/close semantics; if defaults, telemetry, or logs still show `relay_direct`, treat that as an implementation mismatch to be aligned, not as authority over the product statement
- `implementation_note`: when aligning firmware, update `pump_control_mode`, reported `cp`, channel binding `io_kind/resource_ref`, and the real meter-side breaker execution path together in one release; do not claim meter-controlled behavior without a real executable meter-side action path
- `verification`: an aligned release should report meter-control semantics consistently and should execute breaker close/open through the meter-side action path instead of local relay GPIO
- `supersedes`: `EXP-0013`
- `superseded_by`: `none`

### EXP-0015

- `id`: `EXP-0015`
- `title`: `Card swipe audit must be decoupled from business flow and must record every swipe-class event`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `card reader audit trail`
- `problem`: the previous implementation only reported some rejected or auth-failed swipes, while debounce, parse-invalid, offline, and accepted reader events were not guaranteed to leave a consistent upstream trail
- `wrong_assumption`: card-reader reporting can stay attached to business-flow milestones and still satisfy an audit requirement for all swipe events
- `evidence`:
  - [workflow_card_reader.c](/D:/20251211/智能体/hardware/new/code/firmware/workflow/workflow_card_reader.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
- `current_rule`: card swipe流水 is an audit stream, not a business-flow stream; every swipe-class event must leave an audit record, including accepted, rejected, debounced, offline-blocked, and parse-invalid events
- `implementation_note`: enqueue swipe audit records inside the card-reader workflow itself, flush them independently of business start/stop logic, and keep auth-result reporting as a separate business result channel
- `verification`: repeated swipes, debounce drops, offline swipes, checksum failures, and normal accepted swipes all create `ER` audit events through the same queue-driven path
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0016

- `id`: `EXP-0016`
- `title`: `Start actions must never be replayed, while stop outcomes may be resent and unsafe stop states must enter recovery lock`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `safety flow and outage handling`
- `problem`: a disconnected TCP link can delay reporting, and a pump or breaker stop can become uncertain during power anomalies; replaying a start after reconnect is unsafe, and treating an uncertain stop as completed can create silent water release after power returns
- `wrong_assumption`: reconnect recovery can treat start and stop symmetrically, or a stop request may be marked successful even when the physical breaker open result is uncertain
- `evidence`:
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
  - [storage_runtime.c](/D:/20251211/智能体/hardware/new/code/firmware/storage/storage_runtime.c)
  - field scenario clarified on `2026-04-11`: farmer may leave after a swipe, and later power restoration must not silently resume water
- `current_rule`: never replay a start after reconnect; only resend stop-side reports and reconciliation signals. If stop execution is uncertain, or if a running session hits offline-expiry or pump-power-loss, move the workflow into `RECOVERY_LOCKED`, keep the session context persisted, and keep retrying safe-off until the actuator state is confirmed safe
- `implementation_note`: `stop_session_internal` must not return a fake success when pump safe-off fails; instead it should persist an unsafe-stop context, block new starts, and retry local safe-off from the recovery path until success is observed
- `verification`: after network outage or pump-power-loss, the device never auto-resumes watering; after an uncertain stop, the device reports recovery lock, keeps blocking starts, and retries safe-off instead of finalizing a stop as successful
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0017

- `id`: `EXP-0017`
- `title`: `Offline watering runtime must hard-stop within five minutes by default`
- `status`: `active`
- `date`: `2026-04-11`
- `scope`: `network-loss safety policy`
- `problem`: when the platform link is lost during irrigation, letting the session continue for too long increases the chance of unobserved watering, especially if the farmer has already left
- `wrong_assumption`: a long generic offline runtime window is acceptable as a safe default for field irrigation control
- `evidence`:
  - product clarification on `2026-04-11`: `离线超过5分钟就要停止水泵`
  - [config_store.c](/D:/20251211/智能体/hardware/new/code/firmware/storage/config_store.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
- `current_rule`: default `offline_max_runtime_sec` must be `300`, and once exceeded during a running session the device must stop watering through the recovery-safe path rather than continuing indefinitely
- `implementation_note`: keep the limit configurable through platform config, but ship the default as five minutes and combine it with recovery lock so power restoration cannot silently resume watering
- `verification`: a running session with lost online state transitions into stop handling after 300 seconds and enters recovery lock if stop safety is uncertain
- `supersedes`: `none`
- `superseded_by`: `none`

## 8. Superseded Entry Rule

If an old rule is corrected:

- old entry: `status = superseded`
- old entry: `superseded_by = EXP-00NN`
- new entry: `supersedes = EXP-00MM`
- new entry records the evidence that changed the rule

Do not rewrite history to make the old mistake disappear.

## 9. Maintenance Rule

Whenever a new lesson is added, also consider whether these documents must be updated:

- [FIRMWARE_MODULAR_DEVELOPMENT_STANDARD_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/FIRMWARE_MODULAR_DEVELOPMENT_STANDARD_V1_0_0.md)
- [PLATFORM_DEVICE_COMMAND_CONTRACT_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PLATFORM_DEVICE_COMMAND_CONTRACT_V1_0_0.md)
- [PRODUCTION_MINIMAL_BUILD_CHECKLIST_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PRODUCTION_MINIMAL_BUILD_CHECKLIST_V1_0_0.md)
- [PROTOCOL_SOURCE_OF_TRUTH_V1_0_0.md](/D:/20251211/智能体/hardware/new/code/firmware/PROTOCOL_SOURCE_OF_TRUTH_V1_0_0.md)

## 10. Change Log

### v1.0.0

- created the explicit firmware experience library
- froze the rule that experience can be corrected without deleting historical mistakes
- added initial active entries for protocol governance, capability semantics, serialization, modem receive framing, and sensor-free safety defaults
- added power-domain and mixed-source telemetry lessons from relay-direct pump debugging
- added the production rule that virtual fallback data is not allowed by default
