# Platform Device Command Contract

Version: `v1.0.0`
Date: `2026-04-11`
Status: active
Audience: platform engineers, API engineers, AI agents
Scope: commands and queries that the platform is required to send to device firmware

Canonical protocol source:

- [http://xupengpeng.top/ops/interface-protocols#device-protocols](http://xupengpeng.top/ops/interface-protocols#device-protocols)

## 1. Platform Must Separate Internal DTO and Wire Payload

Platform may keep rich internal business objects.

Examples of internal-only fields:

- `source`
- `target_device_id`
- `device_command_token`
- `module_instance_code`
- `channel_code`
- `command_status`

These fields must not be copied into device `wire_message.p` unless they are explicitly part of the wire contract.

## 2. Device Wire Envelope

Platform must send only this envelope:

```json
{"v":1,"t":"SC/QR/EX","i":"imei","m":"msg_id","s":123,"c":"cid","r":"session_ref","p":{}}
```

Rules:

- only `v/t/i/m/s/c/r/p` are valid top-level fields
- `r` may be omitted or `null`
- `c` should be stable enough for reply correlation
- `m` must be unique per wire message
- firmware accepts only short-code payload keys and short-code values; do not send long aliases such as `scope`, `query_code`, `action_code`, `prompt_code`, `common`, `open_valve`, or `reboot_device`

## 3. Platform Must Send Only These Inbound Types

- `SC`
- `QR`
- `EX`

Do not send:

- `AK`
- `QS`
- `RA`
- `RN`

## 4. QUERY Contract

### Allowed payload keys

- `sc`
- `qc`
- `tr`
- `pm`

### Current released queries

#### Query common status

```json
{
  "v": 1,
  "t": "QR",
  "i": "861295087573980",
  "m": "msg-qcs-001",
  "s": 1001,
  "c": "query-001",
  "p": {
    "sc": "cm",
    "qc": "qcs"
  }
}
```

#### Query workflow state

```json
{
  "v": 1,
  "t": "QR",
  "i": "861295087573980",
  "m": "msg-qwf-001",
  "s": 1002,
  "c": "query-002",
  "p": {
    "sc": "wf",
    "qc": "qwf"
  }
}
```

#### Query electric meter

```json
{
  "v": 1,
  "t": "QR",
  "i": "861295087573980",
  "m": "msg-qem-001",
  "s": 1003,
  "c": "query-003",
  "p": {
    "sc": "cm",
    "qc": "qem"
  }
}
```

#### Query upgrade status

```json
{
  "v": 1,
  "t": "QR",
  "i": "861295087573980",
  "m": "msg-qgs-001",
  "s": 1004,
  "c": "query-004",
  "p": {
    "sc": "cm",
    "qc": "qgs"
  }
}
```

#### Query upgrade capability

```json
{
  "v": 1,
  "t": "QR",
  "i": "861295087573980",
  "m": "msg-qgc-001",
  "s": 1005,
  "c": "query-005",
  "p": {
    "sc": "cm",
    "qc": "qgc"
  }
}
```

### Device reply expectations

- success returns `QS`
- unsupported or malformed returns `NK`

### Current `qem` reply shape

```json
{
  "v": 1,
  "t": "QS",
  "i": "861295087573980",
  "m": "000123",
  "s": 123,
  "c": "query-003",
  "p": {
    "mp": "dlt645_2007",
    "vv": 220.1,
    "ia": 0.0,
    "pw": 0.0,
    "ek": 8.1
  }
}
```

### Current `qgs` reply shape

```json
{
  "v": 1,
  "t": "QS",
  "i": "861295087573980",
  "m": "000124",
  "s": 124,
  "c": "query-004",
  "p": {
    "sc": "cm",
    "qc": "qgs",
    "ota_stage": "idle",
    "ota_state": 0,
    "download_progress_pct": 0,
    "write_progress_pct": 0,
    "last_result": 0,
    "last_error_code": 0,
    "current_version": "0.1.23",
    "target_version": "",
    "package_etag": ""
  }
}
```

### Current `qgc` reply shape

```json
{
  "v": 1,
  "t": "QS",
  "i": "861295087573980",
  "m": "000125",
  "s": 125,
  "c": "query-005",
  "p": {
    "sc": "cm",
    "qc": "qgc",
    "ota_supported": 1,
    "min_battery_soc_default": 0,
    "min_signal_csq_default": 0,
    "package_formats": ["raw_bin"],
    "compression_formats": []
  }
}
```

## 5. EXECUTE_ACTION Contract

### Allowed payload keys

- `sc`
- `ac`
- `tr`
- `pm`

### Current released actions

#### Play prompt

```json
{
  "v": 1,
  "t": "EX",
  "i": "861295087573980",
  "m": "msg-ppu-001",
  "s": 1101,
  "c": "cmd-001",
  "p": {
    "sc": "cm",
    "ac": "ppu",
    "pm": {
      "pc": "platform_prompt"
    }
  }
}
```

#### Pause session

```json
{
  "v": 1,
  "t": "EX",
  "i": "861295087573980",
  "m": "msg-pas-001",
  "s": 1102,
  "c": "cmd-002",
  "p": {
    "sc": "wf",
    "ac": "pas"
  }
}
```

#### Resume session

```json
{
  "v": 1,
  "t": "EX",
  "i": "861295087573980",
  "m": "msg-res-001",
  "s": 1103,
  "c": "cmd-003",
  "p": {
    "sc": "wf",
    "ac": "res"
  }
}
```

#### Breaker close

```json
{
  "v": 1,
  "t": "EX",
  "i": "861295087573980",
  "m": "msg-spu-001",
  "s": 1104,
  "c": "cmd-004",
  "p": {
    "sc": "md",
    "ac": "spu",
    "tr": "pump_1"
  }
}
```

#### Breaker open

```json
{
  "v": 1,
  "t": "EX",
  "i": "861295087573980",
  "m": "msg-tpu-001",
  "s": 1105,
  "c": "cmd-005",
  "p": {
    "sc": "md",
    "ac": "tpu",
    "tr": "pump_1"
  }
}
```

#### Remote reboot MCU

```json
{
  "v": 1,
  "t": "EX",
  "i": "861295087573980",
  "m": "msg-rbt-001",
  "s": 1106,
  "c": "cmd-006",
  "p": {
    "sc": "cm",
    "ac": "rbt",
    "tr": "controller"
  }
}
```

### Remote reboot semantics

- released wire short code is `cm/rbt`
- `tr` may be omitted or set to `controller`
- device must return `AK` first, with `p.stg="scheduled"` when reboot has been armed
- actual MCU reset must happen only after the ACK frame has been sent successfully
- device must return `NK` with `rc=DEVICE_BUSY` while OTA workflow is active
- device must return `NK` with `rc=DEVICE_BUSY` while an irrigation session is starting, running, or stopping

## 6. Current Unsupported Actions

Platform must not treat these as released:

- `breaker_open`
- `breaker_close`
- `open_breaker`
- `close_breaker`
- `set_vfd_run`
- `set_vfd_frequency`
- `sync_clock`

If sent, device will return `NK`.

`reboot_device` is not accepted by firmware. Platform must send only `rbt`.

## 7. SYNC_CONFIG Contract

### Allowed payload keys

- `cv`
- `fm`
- `rr`
- `pc`
- `cc`

### Required field

- `cv`

### Allowed released feature codes in `fm`

- `pay`
- `cdr`
- `ebr`
- `bkr`
- `bkf`

Do not send these into `fm` for this board unless a future valve variant is explicitly released:

- `svl`
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

## 8. Current Real Telemetry Surface

Platform should expect:

### `RG`

- `hs`
- `hr`
- `ff`
- `fv`
- `cv`
- `fm`
- `mp` when `ebr` is exposed
- `cp` when `bkr` is exposed

### `HB`

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

### `SS`

- `wf`
- `rt`
- `mp` when `ebr` is exposed
- `vv`
- `ia`
- `pw`
- `ek`
- `brs` only when `bkf` is exposed
- `ch`

## 9. Standard NK Contract

Device `NK` reply format:

```json
{
  "v": 1,
  "t": "NK",
  "i": "861295087573980",
  "m": "000124",
  "s": 124,
  "c": "original-correlation-id",
  "p": {
    "rc": "UC/PI/CE/CV/LB/DB",
    "msg": "reason"
  }
}
```

Current short reject codes:

- `UC`
- `PI`
- `CE`
- `CV`
- `LB`
- `DB`
- `PR`

## 10. Platform Must Provide Before New Capability Is Started

For every new capability request, platform must provide:

- exact wire action or query short code
- exact payload short keys
- whether it is released, gray, or internal-only
- expected `AK`, `QS`, or `NK` examples
- whether the capability should open a user-facing page entry

If platform cannot provide these, firmware should not expose the capability to production `fm`.

## 11. Extension Request Template

When platform wants a new feature, provide the request using this structure:

```text
Capability name:
Board variants:
Release target:
Feature code:
Need query: yes/no
Need action: yes/no
Query short code:
Action short code:
Expected success payload:
Expected NK cases:
Whether page entry should open from fm:
Whether this is released or hidden:
```

## 12. Current Board Reminder

This board currently has:

- DLT645 electric meter read
- breaker-based pump supply control
- no released valve control on the current production variant
- released breaker control
- no released sensor capability surface

## 13. Change Log

### v1.0.0

- froze current platform wire contract
- froze current released queries and actions
- froze platform internal field separation rule
