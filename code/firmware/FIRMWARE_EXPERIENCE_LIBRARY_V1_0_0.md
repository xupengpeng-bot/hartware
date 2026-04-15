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

Experience update is automatic, not optional.

When a thread establishes any new embedded conclusion that should survive the current chat, update this library in the same working round. This includes:

- a newly confirmed root cause
- a corrected wrong assumption
- a reusable debugging workflow
- a protocol interpretation correction
- a safety or recovery rule
- a build, flash, OTA, or resume constraint

Do not defer these updates to "later cleanup". If a separate dated debug note is created, add or update the matching experience entry in the same delivery round so a new thread can recover the lesson from this library first.

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

### EXP-0018

- `id`: `EXP-0018`
- `title`: `Replies under stress need both top-level correlation and payload self-description`
- `status`: `active`
- `date`: `2026-04-12`
- `scope`: `query and action reply correlation`
- `problem`: field stress tests showed commands remaining in `sent` even when the device had already emitted `QS`, `AK`, or `NK`; top-level `c` alone was not sufficient for stable platform-side resolution when reply payloads were too implicit or when `r` was omitted on success paths
- `wrong_assumption`: as long as the top-level reply carries `c`, the platform will always resolve the command correctly, even if payloads do not echo `sc/qc/ac/tr/wf` and successful paths omit the active session reference
- `evidence`:
  - [EMBEDDED_AI_IMPROVEMENT_INSTRUCTION_20260412.md](/D:/20251211/智能体/hardware/new/code/firmware/EMBEDDED_AI_IMPROVEMENT_INSTRUCTION_20260412.md)
  - [proto_query.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_query.c)
  - [proto_execute_action.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_execute_action.c)
- `current_rule`: `QS`, `AK`, and `NK` must all carry enough information to be self-explanatory under burst traffic: keep top-level `c`, echo `r` whenever an active session exists, and include payload-side `sc/qc/ac/tr/wf` so platform correlation does not depend on a single field or on hidden local state
- `implementation_note`: query replies should echo `sc` and `qc`, action replies should echo `sc`, `ac`, and stable target info, and successful replies should resolve `r` from the active runtime session if the request omitted it
- `verification`: under the `2026-04-12` test round, `qcs/qwf/qem/spu/tpu/pas/res` reply frames now include explicit self-description in addition to the top-level correlation token, reducing the chance that platform leaves them in `sent`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0019

- `id`: `EXP-0019`
- `title`: `Same-card second swipe should stop only after a real start has bound that card to the active session`
- `status`: `active`
- `date`: `2026-04-12`
- `scope`: `card workflow and stop safety`
- `problem`: treating every authorized swipe as an immediately active session token can cause false stop attempts when the platform has not yet actually started irrigation; if no binding survives the real start, the device also cannot honor the product rule that the same card should end the current irrigation
- `wrong_assumption`: the card-auth grant itself is enough to bind an active irrigation card, or the card workflow can ignore token binding and still support same-card stop reliably
- `evidence`:
  - [workflow_card_reader.c](/D:/20251211/智能体/hardware/new/code/firmware/workflow/workflow_card_reader.c)
  - [workflow_local_access.c](/D:/20251211/智能体/hardware/new/code/firmware/workflow/workflow_local_access.c)
  - [safety_flow.c](/D:/20251211/智能体/hardware/new/code/firmware/safety/safety_flow.c)
- `current_rule`: bind the active swipe token only after a real start succeeds, clear it after a real stop succeeds, and use that binding to interpret a same-card second swipe as a stop request
- `implementation_note`: keep swipe audit and platform auth as separate streams, preserve the product rule by reusing a lightweight active-token binding, and route second-swipe stop through the real `stop_pump` safety path instead of a fake local state-only stop
- `verification`: once a card-auth-approved irrigation session has truly started, the same card can trigger a stop through the real stop chain; before an actual start exists, the same card does not falsely stop a non-existent running session
- `supersedes`: `none`
- `superseded_by`: `none`

## EXP-0020

- `date`: `2026-04-12`
- `title`: `Remote upgrade must ACK only after the device has really accepted the upgrade workflow`
- `status`: `active`
- `problem`: remote upgrade commands are easy to implement like ordinary control commands, but OTA differs in one important way: `AK` means the device accepted the upgrade workflow, while final success is reported later by stage events
- `wrong_assumption`: returning `AK` as soon as `upg` is parsed is enough, or download/install success can be implied from the same `AK`
- `evidence`:
  - [proto_execute_action.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_execute_action.c)
  - [proto_event_report.c](/D:/20251211/智能体/hardware/new/code/firmware/protocol/proto_event_report.c)
  - [proto_ota.c](/D:/20251211/智能体/hardware/new/code/protocol/proto_ota.c)
  - [app_main.c](/D:/20251211/智能体/hardware/new/code/firmware/app/app_main.c)
- `current_rule`: `upg` must first pass local acceptance checks and successfully enter the OTA workflow before returning `AK`; after that, progress and final results are reported asynchronously as upgrade fact events, and old upgrade tokens must not be replayed
- `implementation_note`: keep OTA acceptance and OTA completion separate, reuse the existing `proto_ota` state machine for precheck/download lifecycle, carry `upgrade_token` end-to-end, and report only upgrade facts after reconnect rather than replaying old `upg`
- `verification`: accepted upgrades emit `command_acked`, active upgrades emit stage reports, duplicate `upgrade_token` returns explicit `NK`, and reconnect behavior only resends upgrade fact reports
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0021

- `id`: `EXP-0021`
- `title`: `OTA resume must not hard-fail on recoverable 206 header parsing when size and SHA can still prove correctness`
- `status`: `active`
- `date`: `2026-04-14`
- `scope`: `OTA HTTP range resume`
- `problem`: field OTA runs reached `AK` and progressed deeply into download, but failed around `56%` to `90%` with `content_length_mismatch` even though the server returned legal `206 Partial Content` responses for the requested range
- `wrong_assumption`: any `Content-Range` parsing irregularity or partial header detail mismatch should immediately abort the upgrade as `content_length_mismatch`
- `evidence`:
  - [EMBEDDED_DEBUG_OTA_NOTES_20260414.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/EMBEDDED_DEBUG_OTA_NOTES_20260414.md)
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c)
  - live verification on `2026-04-14` that `Range: bytes=64814-` against the public artifact returned legal `HTTP/1.1 206 Partial Content`, matching `Content-Length` and `Content-Range`
- `current_rule`: keep `status`, `ETag`, manifest size, final byte count, and `SHA256` strict; treat recoverable `206` header parsing issues as a fallback path rather than an immediate hard failure when the returned body can still be validated safely
- `implementation_note`: use bounded header-line scanning, avoid cross-body searches, and only hard-fail on true contradictions such as wrong status, wrong `ETag`, impossible byte range, or final size/hash mismatch
- `verification`: OTA should no longer die at deep resume offsets with `content_length_mismatch` when the server response is otherwise valid; any remaining failure should move to a more accurate stage such as `size_mismatch`, `sha256_mismatch`, `flash_write_failed`, or complete successfully to `boot_confirmed`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0022

- `id`: `EXP-0022`
- `title`: `OTA HTTP transport must preserve the exact byte stream and must not invent alignment at the download layer`
- `status`: `active`
- `date`: `2026-04-14`
- `scope`: `OTA HTTP body delivery, SHA256, flash write path`
- `problem`: field OTA runs progressed to `100%` and then failed with `sha256_mismatch` even after command delivery, ACK accounting, package size, and `206 Partial Content` header handling were fixed
- `wrong_assumption`: the HTTP download layer should normalize body output to even-byte chunks for flash safety, and it is safe to defer one trailing byte between download calls before SHA and flash see it
- `evidence`:
  - [proto_ota.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/protocol/proto_ota.c)
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c)
  - [stm32f1_flash.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/platform/stm32f1_flash.c)
- `current_rule`: the HTTP OTA layer must return the exact downloaded byte stream to the OTA state machine; any flash alignment requirement must be handled only inside the flash write path, never by mutating the bytes that feed `SHA256`
- `implementation_note`: `proto_ota` hashes the bytes before flash write, and `stm32f1_flash_program()` already accepts odd-length writes by padding the final halfword with `0xFF`; therefore HTTP-layer odd-byte deferral or reordering is unnecessary and can create size-correct but hash-wrong streams
- `verification`: after this fix, OTA should stop failing at `100%` with `sha256_mismatch` caused by local byte-stream mutation; remaining failures, if any, should point to the real network body content or server-side artifact rather than transport-layer alignment logic
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0023

- `id`: `EXP-0023`
- `title`: `STM32F103 OTA staging writes should be buffered and page-verified instead of streaming many small direct flash writes`
- `status`: `active`
- `date`: `2026-04-14`
- `scope`: `OTA staging write path, STM32F1 flash reliability, resume tail-page failures`
- `problem`: after command delivery, `AK`, package size, `206` handling, and byte-exact SHA transport were fixed, field OTA still failed late with `flash_write_failed` around `99%`
- `wrong_assumption`: once the staging slot is erased, directly programming every downloaded `256-byte` chunk into flash is always reliable enough, and late write failures must come from size or checksum logic
- `evidence`:
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c)
  - [stm32f1_flash.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/platform/stm32f1_flash.c)
  - platform upgrade job `f2d3672a-8943-4cb4-8ba9-72922911da50`, which reached `acked` and then failed with `flash_write_failed` at `99%` while the package size remained safely inside the staging slot
- `current_rule`: write the staging image with a page-sized RAM buffer, flush full pages or the final tail page explicitly, and verify flash contents after each page program; if a page write fails, re-erase that page and retry once before surfacing a hard OTA write failure
- `implementation_note`: on STM32F103, page erase is `2 KiB`; buffering by page reduces repeated small write transactions, avoids fragile tail-page behavior, and makes any retry deterministic because the full page image is already available in RAM
- `verification`: after this fix, late OTA write failures should stop surfacing as generic `flash_write_failed`; any remaining failure should point to a specific page flush, offset mismatch, or a later boot-control stage
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0024

- `id`: `EXP-0024`
- `title`: `OTA HTTP on EC801 must not trust single-field QIRD tails without an immediate unread probe`
- `status`: `active`
- `date`: `2026-04-14`
- `scope`: `EC801 QIRD single-field responses during OTA HTTP download`
- `problem`: even after fixing `206` handling and byte-exact delivery, field OTA could still reach `100%` and fail with `sha256_mismatch` while the device was already running the transport-fix firmware
- `wrong_assumption`: the HTTP download path can rely on the coarse `+QIURC: "recv"` hint or on the single-field `+QIRD: <n>` response alone to derive the unread tail after each fetch
- `evidence`:
  - [net_4g_modem.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_4g_modem.c)
  - platform TCP audits for upgrade token `22f2f0aa-a873-4067-b8ee-a0f548b20eba`, which reached `AK` and then failed with `sha256_mismatch` at `100%` while runtime firmware was `0.1.29`
- `current_rule`: after every successful single-field `QIRD` fetch, immediately query `AT+QIRD=0,0` and use its unread field as the authoritative pending tail for both business TCP and OTA HTTP sessions
- `implementation_note`: this extra probe is cheap compared with a failed OTA and prevents hidden unread tails from being dropped when the modem returns only `+QIRD: <n>` instead of the three-field form
- `verification`: after this fix, a firmware already containing the byte-exact HTTP transport change should stop failing at `100%` with `sha256_mismatch` purely due to hidden unread tails in the EC801 receive path
- `supersedes`: `none`
- `superseded_by`: `none`

## 8. Superseded Entry Rule

### EXP-0025 | Legacy OTA dispatch must preserve standard strong ETag quotes

- `status`: `active`
- `category`: `ota / backend-interop / release-dispatch`
- `problem`: field devices still running the legacy downloader can reject a valid OTA immediately at `0%` with `etag_mismatch` even though the artifact endpoint returns a correct strong `ETag`
- `wrong_assumption`: stripping the quotes from a strong HTTP `ETag` before putting it into the OTA manifest is harmless because newer firmware can normalize quoted and bare variants
- `evidence`:
  - [firmware.module.ts](/D:/Develop/houji/houjinongfuAI-Cursor/houjinongfuai-working/backend/src/modules/firmware/firmware.module.ts)
  - OTA job `1290ec6e-6e52-4e86-81a8-55f37b3b7f3c`, which failed at `0%` with `etag_mismatch` while the command payload carried a bare hash and the artifact endpoint still returned `ETag: "79f94338efc65152a53f2a11bfbc5c463fbef2c52c9fe28fb4a7cca5ecea2277"`
- `current_rule`: OTA dispatch metadata must preserve the canonical strong `ETag` form from the artifact endpoint; do not strip quotes in backend release dispatch
- `implementation_note`: local modern downloaders may normalize quoted or bare inputs, but the backend must stay compatible with field devices that compare against the exact strong-header form
- `verification`: after restoring quoted strong `ETag` dispatch, the same legacy field downloader should stop failing at `0%` with `etag_mismatch` for the same artifact URL and checksum
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0026 | Fixed downloader must be seeded before next-hop OTA validation counts

- `status`: `active`
- `category`: `ota / field-validation / downloader-bootstrap`
- `problem`: once metadata bugs like bare-`ETag` dispatch are fixed, a device still running an older downloader can continue to fail a new package at `100%` with `sha256_mismatch`, making repeated OTA retries look like the latest downloader fix did nothing
- `wrong_assumption`: if the latest package contains the downloader fix, repeatedly OTA-ing that package from an older runtime is enough to validate the fix
- `evidence`:
  - OTA job `92564091-f80e-41cb-862b-f60b69c04589`, which no longer failed at `0%` with `etag_mismatch` after restoring quoted strong `ETag`, but still failed at `100%` with `sha256_mismatch` while runtime firmware remained `0.1.29`
  - [scan_trial_defs.h](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/config/scan_trial_defs.h), where the fixed downloader runtime was advanced beyond `0.1.29`
  - [tmp_gui_serial_COM4_20260414_102754.remote.txt](/D:/Develop/houji/houjinongfuAI-Cursor/tmp_gui_serial_COM4_20260414_102754.remote.txt), where one capture window contained two independent `AK`-accepted OTA attempts from runtime `0.1.29` plus one replay of the same command token, yet every reconnect still registered `fv":"0.1.29"` and never showed the newer downloader runtime
- `current_rule`: when the active field runtime still uses a known-buggy downloader, validate in two hops: first seed the fixed downloader manually, then run OTA to a newer target so the fixed downloader is the one performing the download
- `implementation_note`: repeated OTA retries from the same old runtime, even when the command reaches `AK`, are not fresh validation of the target package; they mostly reconfirm that the still-running downloader is the component performing the broken download. For this thread, the currently available fixed downloader seed is `0.1.32`; after burning it manually, validate OTA with a newer target such as `0.1.33`
- `verification`: only treat the downloader fix as validated after the device runtime shadow first shows the fixed seed version and a subsequent OTA to a newer release completes without the old checksum failure
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0027 | Version bumps must reject mixed-version incremental firmware builds

- `status`: `active`
- `category`: `build / release-integrity / versioning`
- `problem`: after bumping the runtime firmware version, a release BIN can still behave like the old version on the wire because only part of the codebase was rebuilt
- `wrong_assumption`: if `main.c` prints the new version and the final BIN file timestamp is fresh, the whole firmware image must already be version-consistent
- `evidence`:
  - [common_identity.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/common/common_identity.c), which populates the runtime identity and register `fv`
  - [scan_trial_defs.h](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/config/scan_trial_defs.h), which was bumped to `0.1.32`
  - local binary inspection on `2026-04-14 11:20 CST`, where `controller_fw.bin` simultaneously contained `0.1.29` and `0.1.32`, and `common_identity.c.obj` still carried the old version while `main.c.obj` already carried the new one
- `current_rule`: every version bump must either force a clean rebuild or explicitly depend on the version header for all direct consumers, and the final BIN must be checked to ensure it contains only the intended runtime version string
- `implementation_note`: in this workspace, adding `OBJECT_DEPENDS` from `firmware/common/common_identity.c` and `firmware/app/main.c` to `firmware/config/scan_trial_defs.h`, then doing a clean rebuild, eliminated the mixed-version artifact and produced a BIN that only contained `0.1.32`
- `verification`: if the build is correct, `common_identity.c.obj`, `libcontroller_firmware.a`, and the final `controller_fw.bin` should no longer contain the previous runtime version string
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0028 | OTA HTTP must not share the generic modem FIFO/poll receive path

- `status`: `active`
- `category`: `ota / modem-receive / integrity`
- `problem`: a device already running the fixed downloader bridge could still complete an OTA download to `100%` and then fail with `sha256_mismatch`, even though release metadata, strong `ETag`, ACK bookkeeping, package size, and runtime version were all correct
- `wrong_assumption`: it is safe for OTA HTTP download to reuse the same modem receive machinery as business TCP, including background `net_4g_modem_poll()`, `QIRD` active probes, pending-length heuristics, and the shared TCP FIFO
- `evidence`:
  - [proto_ota.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/protocol/proto_ota.c), where SHA256 is computed on downloaded chunks before flash write
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c), which previously consumed OTA HTTP body bytes through `net_4g_modem_tcp_rx_pop()`
  - [net_connectivity.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_connectivity.c), which previously still called `net_4g_modem_poll()` while paused for OTA
  - field validation on `2026-04-14 11:34 CST`, where `0.1.32 -> 0.1.33` reached `AK` and `100%` but still failed with `sha256_mismatch` after all metadata-side issues were already closed
- `current_rule`: during OTA HTTP, the downloader must own modem receive state exclusively; business connectivity polling and the shared TCP FIFO must not touch the same live HTTP socket
- `implementation_note`: the landed fix introduces a direct `AT+QIRD` fetch path for OTA HTTP in [net_4g_modem.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_4g_modem.c), and [net_connectivity.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_connectivity.c) now returns before `net_4g_modem_poll()` when `s_paused_for_ota != 0`
- `verification`: once the fixed seed runtime is flashed, the next-hop OTA should stop failing at `100%` with a size-correct but hash-wrong stream that originated only inside the modem receive path
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0029 | OTA pause handoff must not trust stale port-local bookkeeping

- `status`: `active`
- `category`: `ota / control-handoff / retry-session-ownership`
- `problem`: after one OTA attempt finishes and connectivity resumes, the next accepted OTA in the same runtime can skip the expected `ota_pause` transition and immediately flood `[PROTO] RX invalid prefix ... shift=1 for resync`, even when the downloader runtime and package SHA are correct
- `wrong_assumption`: the OTA port-local flag `s_http.paused_network` is a reliable source of truth for whether the control stack has already yielded modem ownership to the OTA HTTP session
- `evidence`:
  - [tmp_gui_serial_COM4_20260414_112540.remote.txt](/D:/Develop/houji/houjinongfuAI-Cursor/tmp_gui_serial_COM4_20260414_112540.remote.txt), where runtime `0.1.32` showed one normal `AK -> ota_pause` OTA followed by a second accepted OTA without a fresh `ota_pause`, and then a long `RX invalid prefix` storm
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c), where the OTA port previously used `s_http.paused_network` as the gate for whether to call `net_connectivity_pause_for_ota()`
  - [app_main.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/app/app_main.c), where OTA terminal events resume connectivity through `net_connectivity_resume_after_ota()` outside the OTA port, which can leave the OTA port-local flag stale
  - local SHA verification on `2026-04-14 11:42 CST`, where the device-equivalent OTA SHA256 logic matched standard SHA256 on boundary vectors and on `_verify_0_1_33.bin`, ruling out the SHA core as the cause of this latest symptom
- `current_rule`: every OTA HTTP session open must reassert modem ownership through `net_connectivity_pause_for_ota()` before the modem socket is reused, regardless of any previous value stored in an OTA port-local bookkeeping flag
- `implementation_note`: the safe pattern is to treat `s_http.paused_network` as bookkeeping only and always delegate pause/resume to the connectivity layer, because only that layer also clears `s_sock.connected` and prevents business RX from consuming HTTP response bytes
- `verification`: after an OTA failure or reconnect, the next accepted OTA attempt should log a fresh `ota_pause` before HTTP download begins, and the control path should no longer see `RX invalid prefix` storms caused by raw HTTP bytes being parsed as business frames
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0030 | OTA pause must leave a flush window for accepted uplinks

- `status`: `active`
- `category`: `ota / transport / ack-accounting`
- `problem`: a fixed downloader runtime can locally emit `AK` and `ER/upg accepted`, yet the platform still leaves the OTA job in `command_sent_waiting_ack` because the device closes the control TCP session immediately afterward
- `wrong_assumption`: once `net_connectivity_send_json()` returns success for the accepted OTA ACK/event, the modem has had enough time to flush those frames before `ota_pause` disconnects the business socket
- `evidence`:
  - [gui_serial_COM4_20260414_115538.txt](https://volume-received-disturbed-employ.trycloudflare.com/gui_serial_COM4_20260414_115538.txt), where runtime `0.1.34` logged `AK`, `ER/upg accepted`, and then `disconnect ctx reason=ota_pause ... last_tx=ER/378B 0 ms ago`
  - platform job `9489958a-3395-4467-b829-a1abb259f5a9`, which still stayed at `command_sent_waiting_ack` even though the device-side serial showed those frames being queued locally
  - [net_connectivity.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_connectivity.c), which previously disconnected immediately in `net_connectivity_pause_for_ota()` with no grace period after the last uplink
- `current_rule`: before OTA steals modem ownership, the control path must leave a short flush window after the latest uplink so `AK` and `ER/upg accepted` can actually leave the modem and reach the server
- `implementation_note`: the landed fix adds an `800 ms` flush guard in `net_connectivity_pause_for_ota()` whenever the latest uplink frame is still fresh, and logs the wait so field traces can confirm the guard fired
- `verification`: the next accepted OTA attempt should show `[PROTO] ota_pause flush guard ...` before `ota_pause`, and the platform should stop leaving the job in `command_sent_waiting_ack`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0031 | Mandatory package SHA256 makes HTTP ETag advisory, not authoritative

- `status`: `active`
- `category`: `ota / integrity / http-resume`
- `problem`: a field OTA can reach deep resume offsets and still fail early with `etag_mismatch`, even though the artifact endpoint returns a correct quoted strong `ETag` on both `200` and `206`, and the package already carries a mandatory final `SHA256`
- `wrong_assumption`: response `ETag` must remain a hard failure gate throughout the OTA session, including resumed `206 Partial Content` requests, even when final package SHA256 verification is already guaranteed
- `evidence`:
  - backend job `cc93d9f9-d512-4b4c-a1c9-3cd8c3c06584`, which progressed to `85%` and then failed with `etag_mismatch`
  - [gui_serial_COM4_20260414_121141.txt](https://volume-received-disturbed-employ.trycloudflare.com/gui_serial_COM4_20260414_121141.txt), where runtime `0.1.35` accepted the OTA and advanced well past the initial request before the platform recorded `etag_mismatch`
  - direct artifact checks on `2026-04-14 12:18 CST`, where both `200 OK` and `206 Partial Content` returned the same quoted strong `ETag`, correct `Content-Range`, and correct `Content-Length`
  - [proto_execute_action.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/protocol/proto_execute_action.c), which already requires both `package_sha256_hex` and `package_etag` to be present before OTA starts
  - [proto_ota.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/protocol/proto_ota.c), where final verification still rejects any byte-stream mismatch with `sha256_mismatch`
- `current_rule`: once package SHA256 is mandatory, `ETag` is advisory transport metadata only; response `ETag` parse or compare failures must not abort an OTA session by themselves
- `implementation_note`: keep the first-line HTTP sanity checks (`status`, `Content-Type`, `Accept-Ranges`, `Content-Range`, package size) but downgrade manifest/response `ETag` normalization and compare failures to diagnostic logs, leaving final integrity to the mandatory end-to-end SHA256 check
- `verification`: after this fix, OTA should stop failing mid-download with `etag_mismatch` when the server response is otherwise valid; any remaining failure should move to a truer stage such as `sha256_mismatch`, `flash_write_failed`, or complete successfully through `boot_confirmed`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0032 | Local-vs-remote device mode must be confirmed before any real hardware action

- `status`: `active`
- `category`: `workflow / hardware-ops / operator-intent`
- `problem`: embedded threads can drift into the wrong execution model, treating a locally connected board as a remote-log-only target or vice versa, which causes wasted OTA retries, missed local flashing opportunities, and incorrect assumptions about what Codex is allowed to execute directly
- `wrong_assumption`: the thread can infer from prior context whether the current board is local or remote without explicitly reconfirming with the user
- `evidence`:
  - user clarification on `2026-04-14` that local and remote modes can switch between turns and future hardware actions should first confirm which mode is active
  - remote helper portal [https://volume-received-disturbed-employ.trycloudflare.com](https://volume-received-disturbed-employ.trycloudflare.com), which contains not only logs but also flashing and serial helper scripts
  - confirmed local ST-Link flashing tool at `D:\Tools\STLink\bin\st-flash.exe`
- `current_rule`: before flash / erase / reset / serial capture / live-device OTA retry, first confirm whether the session is `local bench` or `remote/log-only`
- `implementation_note`: once the user confirms `local bench`, Codex may directly use the local wrapper `hartware/build_flash/flash.cmd` or the local `st-flash` toolchain with the correct image/address mapping
- `verification`: future threads should stop wasting OTA-only retries when a local board is available, and should stop attempting local hardware operations when the user has switched back to remote-only mode
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0033 | Local-bench `100% sha256_mismatch` must be closed with a staging dump, not only with platform status

- `status`: `active`
- `category`: `ota / local-bench / evidence-preservation`
- `problem`: a local-bench OTA can still fail at `100%` with `sha256_mismatch` even after command delivery, `AK`, `accepted`, `ota_pause`, and prior next-hop success to a newer downloader runtime are already proven
- `wrong_assumption`: once the platform reports `sha256_mismatch` at `100%`, it is already clear enough that the problem is "somewhere in download integrity", so preserving a same-round staging dump can wait until a later investigation
- `evidence`:
  - local OTA validation on `2026-04-14 16:27 CST`, where `0.1.40 -> 0.1.41` was dispatched from the local board session through backend job `18561fda-bdb0-4949-a7ed-8e62ac8f74bc` and failed as `accepted -> failed@100% -> sha256_mismatch`
  - [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt), where the same run showed `AK/accepted`, a fresh `ota_pause`, and later re-registration still at `fv":"0.1.40"` with no `0.1.41` boot in the same capture window
  - [staging_dump_20260414_ota41_failed.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/staging_dump_20260414_ota41_failed.bin), whose first `118536` bytes did not match the exact release artifact and differed in `8717` bytes across `195` diff segments, with the first diff at offset `2149`
- `current_rule`: whenever a local-bench OTA ends with `100% sha256_mismatch`, preserve and compare three artifacts in the same round: the exact release BIN and SHA256, the local serial log slice, and a staging-slot readback from the board
- `implementation_note`: a same-round staging dump turns an abstract checksum failure into byte-level evidence. If the staged bytes already differ from the release artifact, the issue is still inside OTA body assembly, staging write, or their interaction; boot-confirm and post-reboot reporting are not the primary suspects for that run
- `verification`: future local-bench OTA investigations should include release-vs-staging comparison data (artifact hash, staged-prefix hash, diff count, first diff offset) before claiming the failure is "already understood"
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0034 | OTA debug builds must stay below the staging-slot ceiling before bench results are trusted

- `status`: `active`
- `category`: `ota / local-bench / package-size`
- `problem`: a debug-instrumented OTA build can be rejected before download with `NK rc=BZ msg=storage low`, which looks like a new OTA/runtime failure even though the real issue is simply that the artifact no longer fits in the staging slot
- `wrong_assumption`: once a debug build compiles, it is still a valid downloader test sample, so a subsequent `BZ/storage low` result can be interpreted as part of the transport-integrity investigation
- `evidence`:
  - a temporary local diagnostic build on `2026-04-14` grew [controller_fw.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_codex_arm/controller_fw.bin) to `118928` bytes
  - the staging slot capacity is only `118784` bytes
  - the bench device rejected `cmd-90590747-00cc-421d-bb5e-150adf39e00e` with `NK rc=BZ msg=storage low` instead of entering `ota_pause`
- `current_rule`: before interpreting any local OTA result, compare the exact BIN size against `FLASH_STAGING_SLOT_SIZE_BYTES`; if the artifact is larger, treat the run as an invalid packaging test rather than a downloader verdict
- `implementation_note`: keep local diagnostics size-aware. If a temporary logging patch pushes the artifact above the slot ceiling, shrink or revert the instrumentation before resuming integrity analysis
- `verification`: a valid local OTA investigation run must first pass the size gate and reach `accepted` / `ota_pause`
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0035 | HTTP QIRD seam cleanup should discard only partial shared-stream residue, not freshly absorbed UART bytes

- `status`: `active`
- `category`: `ota / modem / qird-seam`
- `problem`: local-bench OTA corruption can still look like "repeat previous byte at the next seam" even after the UART FIFO/DR race is hardened, which means stale modem shared-stream residue is another real contributor
- `wrong_assumption`: if partial control-tail residue is hurting OTA seams, then the safest next step is to absorb any immediately available UART bytes into the shared stream and discard them together
- `evidence`:
  - a conservative cleanup that discarded only incomplete shared-stream residue during HTTP QIRD work reduced staged-image corruption on `2026-04-14` from `8944` diff bytes / `142` segments to `6486` diff bytes / `123` segments in [staging_dump_20260414_ota41_failed_residuefix.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/staging_dump_20260414_ota41_failed_residuefix.bin)
  - the same family of fix became a severe regression once it first called `stream_push_from_uart()` and then discarded the residue: [staging_dump_20260414_ota41_failed_residuefix2.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/staging_dump_20260414_ota41_failed_residuefix2.bin) then differed in `57974` bytes across `4470` segments, with corruption starting at offset `404`
- `current_rule`: during HTTP OTA seam cleanup, only discard already-buffered incomplete shared-stream residue after complete lines have been processed; do not proactively absorb fresh UART bytes into that discard path
- `implementation_note`: a partial-line residue filter can help because it removes stale tail bytes that would otherwise leak into the next `QIRD` seam. But once the cleanup starts pulling fresh UART data first, it can drop valid in-flight modem bytes and massively worsen the staged image
- `verification`: keep the shared-stream-only cleanup, revert the aggressive absorb-and-discard variant in the same round, and verify new OTA attempts against fresh staging dumps rather than relying only on platform state
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0036 | Relay-output debug must separate the symmetric `relay_*` path from the special `pump_1` alias path

- `status`: `active`
- `category`: `relay-output / routing / board-mapping`
- `problem`: when a pump controller page exposes `relay_1` and `relay_2`, it is easy to conclude that `relay_2` has no firmware implementation if it does not move while `relay_1` appears to work
- `wrong_assumption`: `relay_1` and `relay_2` are handled by different firmware logic branches, so a "relay_2 no response" symptom usually means the second relay path is missing or broken in software
- `evidence`:
  - local product firmware `SCAN-IRR-VALVE-CTRL-4G-A01` enables `relay_output_control` by default and binds both `relay_1` and `relay_2` as enabled relay-output channels in `storage/config_store.c`
  - `module_relay_output.c` resolves `relay_1 -> index 0` and `relay_2 -> index 1`, drives both through the same `relay_drive_output()` helper, and uses the same active-level rule for both outputs
  - `proto_execute_action.c` accepts `md + orl/crl + tr=relay_1|relay_2` for the generic relay path, while the pump path still requires `md + spu/tpu + tr=pump_1`
  - board config maps the two relay outputs to different pins (`RELAY1=PC1`, `RELAY2=PC3`), and also aliases `pump_1` onto the relay-1 pump-run pin
- `current_rule`: when `relay_2` does not move while `relay_1` seems normal, first separate which path was actually exercised: generic relay control (`open_relay/close_relay`, `relay_output_control`, `relay_1|relay_2`) or pump control (`start_pump/stop_pump`, `pump_1`). Treat the generic relay path as software-symmetric until evidence proves otherwise
- `implementation_note`: `relay_1` is dual-homed in this product baseline: it can appear to "work" through the special `pump_1` pump-run route even if the generic relay-output path was not the one being tested. `relay_2` is generic-only, so a mismatch between the two often points to board wiring, board variant differences, stale live firmware/config, or missing external output hardware rather than asymmetric relay-module code
- `verification`: future relay debug should confirm four things in order: the dispatched `action_code/module_code/target_ref`, the device ACK/NACK, the relay state in `proto_state_snapshot`, and, for local-bench sessions, the real electrical behavior on the relay-2 output path or its mapped MCU pin
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0037 | Platform pump-control strategy should reuse wire-level `cc` while keeping richer bindings in extension keys

- `status`: `active`
- `category`: `protocol / config-sync / platform-model`
- `problem`: the firmware already parses `cc/control_config`, but platform and frontend can still drift into exposing raw relay resources or inventing new wire enums when trying to model meter-breaker, single-relay, and dual-relay pump control strategies
- `wrong_assumption`: if the platform needs richer pump-control semantics, it should replace the existing wire field `cc.pump_control_mode` with new protocol-level enum values and let the firmware catch up later
- `evidence`:
  - local firmware `proto_sync_config.c` already reads `json_get_item_alias(payload, "cc", "control_config")` and currently recognizes the wire-level `pump_control_mode` values `meter_breaker_485 / relay_direct / contactor_direct`
  - `model_config.h` and `config_store.c` already persist `control_config_t`, so `cc` is a real device config domain rather than a platform-only invention
  - current firmware ignores unknown extension keys inside `cc`, which allows the platform to keep richer binding metadata without breaking existing devices
- `current_rule`: keep `cc.pump_control_mode` wire-compatible for firmware-facing meaning, and place richer platform orchestration data such as `pump_strategy_kind`, `logical_target`, `start_binding`, `stop_binding`, and `feedback_binding` alongside it as extension keys
- `implementation_note`: when a platform change formalizes logical pump control, sync the whole chain together in one round: ledger storage, `SYNC_CONFIG` payload generation, device detail/config UI, and the public protocol document. Do not overload `runtime_rules` with pump-control topology
- `verification`: after platform changes, confirm three layers: ledger detail can read/write `control_config`, queued `SYNC_CONFIG` emits compact `cc`, and existing firmware still accepts the payload because the wire-level `pump_control_mode` value remains compatible
- `supersedes`: `none`
- `superseded_by`: `none`

### EXP-0038 | Dual-relay pulse pump control is currently a platform orchestration pattern, not a new firmware action code

- `status`: `active`
- `category`: `pump-control / relay-pulse / execution-routing`
- `problem`: after adding `dual_relay_pulse` to platform `control_config`, it is tempting to look for a dedicated firmware-side `start_pump_pulse` or `pulse_relay_pair` action and block the platform rollout until such an action exists
- `wrong_assumption`: supporting dual-relay pulse pump control requires a brand-new wire action code before the platform can route logical `start_pump / stop_pump`
- `evidence`:
  - local product firmware accepts generic relay actions through `md + orl/crl + tr=relay_1|relay_2`, and accepts logical pump actions through `md + spu/tpu + tr=pump_1`
  - the current firmware action table does not expose a dedicated pump pulse action; `dual_relay_pulse` lives only in platform-side binding metadata for now
  - platform command queue already has a usable delayed-dispatch mechanism via `retry_pending + transport.next_retry_at`, which can schedule the release edge of a pulse without inventing a new wire protocol
- `current_rule`: treat `dual_relay_pulse` as a platform orchestration strategy that expands logical pump start/stop into relay command sequences, while keeping the wire protocol limited to existing `spu/tpu` and `orl/crl`
- `implementation_note`: for current platform rollout, queue the press edge immediately and schedule the release edge as a delayed follow-up command; keep `meter_breaker_modbus` on firmware-native `start_pump / stop_pump` fallback until a real meter-side execute path exists in firmware
- `verification`: validate three things in order: the gateway queued both pulse edges with the second command delayed, the device detail page now sends logical `start_pump / stop_pump` for configured pump controllers, and the live device receives `orl -> crl` on the expected relay target when the strategy is `dual_relay_pulse`
- `supersedes`: `none`
- `superseded_by`: `none`

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
