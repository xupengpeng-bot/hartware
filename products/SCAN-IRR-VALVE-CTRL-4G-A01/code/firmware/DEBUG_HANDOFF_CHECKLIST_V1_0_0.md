# Debug Handoff Checklist

Version: `v1.0.0`
Date: `2026-04-11`
Status: active
Audience: field debug engineers, test engineers, hardware bring-up engineers, AI agents
Scope: what embedded development requires from debug staff to finish a capability quickly and correctly

## 1. Goal

This checklist prevents repeated back-and-forth during board bring-up.

If the debug side provides the items below once, firmware work can be much faster, safer, and more repeatable.

## 2. Minimum Hardware Information Required

Before firmware starts a new hardware capability, debug staff must provide:

- board model name
- hardware revision
- MCU model
- power topology summary
- exact peripheral connection summary
- UART / RS485 / ADC / GPIO pin allocation
- which features are physically populated on this board

If a feature is not physically populated, do not ask firmware to expose it as a released capability.

## 3. RS485 Device Bring-up Requirements

For any RS485 device such as electric meter, breaker, VFD, or sensor, debug staff must provide:

- device model
- vendor name
- protocol family
  - for example:
    - `dlt645_2007`
    - `modbus_rtu`
    - vendor private frame
- baudrate
- parity
- stop bits
- slave address or addressing rule
- bus wiring confirmation
  - A/B line
  - common ground if needed
  - termination / bias notes if relevant
- RS485 direction control pin
- capture showing one successful exchange if available

## 4. Protocol Material Required

For every new protocol-controlled peripheral, debug staff must provide at least one of:

- official protocol manual PDF
- vendor operation manual with frame examples
- serial debugging transcript
- logic analyzer capture
- PC tool screenshot showing a successful command

For control devices, also provide:

- write command frames
- read-back or feedback frames
- parameter scaling rules
- checksum rules
- any password / operator code / security field requirement

## 5. Required Test Cases From Debug Side

Before firmware integration is declared done, debug staff must run and return results for:

- normal success case
- invalid address or disconnected case
- timeout case
- repeated poll case
- power cycle after configuration case
- reconnect after network interruption case

For control capabilities, also test:

- command accepted
- command rejected by device
- no feedback state
- repeated command while busy
- safe stop or fail-safe after power interruption

## 6. Required Logs

Debug staff must provide logs in raw text form, not screenshot only.

Preferred logs:

- device serial log
- AT log if modem is involved
- RS485 tx/rx log
- platform received JSON log
- platform command send log

Minimum log requirements:

- timestamps if possible
- raw request
- raw reply
- command result
- failure reason

## 7. Required Photos or Physical Confirmation

For hardware ambiguity, debug staff should provide:

- board photo
- RS485 wiring photo
- meter or breaker nameplate photo
- control output wiring photo
- jumper or DIP switch position photo

These save a large amount of guesswork.

## 8. Platform-Linked Debug Information

When a platform command is involved, debug staff must provide:

- platform internal command object
- final device wire message
- correlation token
- platform receive result
- whether page entry was opened by `fm`

Important rule:

- a platform internal object is not enough
- the exact `wire_message` sent to the device is required

## 9. Acceptance Evidence Required Per Capability

For every new capability, debug side should return one compact package containing:

- hardware identity
- protocol material
- exact serial parameters
- one success log
- one failure log
- one platform round-trip log
- one final acceptance statement

Recommended folder package:

```text
capability_name/
  protocol_manual.pdf
  tx_rx_success.txt
  tx_rx_timeout.txt
  platform_command.json
  platform_reply.json
  hardware_photo.jpg
  notes.txt
```

## 10. What Firmware Expects From Platform During Debug

During debug, platform should provide:

- exact compact `wire_message`
- whether the command is production or debug-only
- expected reply shape
- whether feature should already be present in `fm`

If platform only gives long internal fields, debug staff must help extract the real wire payload.

## 11. Bring-up Decision Rules

Firmware can move quickly when debug provides enough material.

### Ready for implementation

All items below are present:

- hardware connection known
- protocol material known
- serial parameters known
- at least one test path known

### Ready for release

All items below are present:

- real device success log
- failure log
- platform round-trip log
- telemetry visible on platform
- `NK` behavior checked for unsupported or invalid input

## 12. Current Board Known Reality

For the current board:

- electric meter is real and uses `dlt645_2007`
- pump control is released
- single valve control is released
- breaker control is not yet released
- sensor display is not yet released

So for future debug requests on this board, do not ask firmware to expose breaker or sensor pages without the real protocol and hardware proof package.

## 13. Quick Request Template For Debug Staff

Use this template when handing a new peripheral to firmware:

```text
Board model:
Hardware rev:
Capability name:
Peripheral model:
Protocol family:
Baudrate/parity/stop:
Address:
Pins used:
Power requirements:
Success frame sample:
Failure frame sample:
Write command sample:
Read-back sample:
Expected platform fields:
Photos/log files attached:
```

## 14. Change Log

### v1.0.0

- established the standard debug handoff package
- froze RS485 bring-up material requirements
- froze debug evidence required for release
