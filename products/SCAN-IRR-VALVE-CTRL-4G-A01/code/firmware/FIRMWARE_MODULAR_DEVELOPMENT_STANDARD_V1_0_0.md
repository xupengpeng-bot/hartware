# Firmware Modular Development Standard

Version: `v1.0.0`
Date: `2026-04-11`
Status: active
Audience: embedded engineers, platform engineers, test/debug engineers, AI agents
Scope: scan-irrigation controller boards and future derivative boards

## 1. Goal

This standard defines a reusable firmware development mode for controller-class boards.

The design target is:

- protocol can be switched or trimmed without rewriting business logic
- hardware changes can be absorbed at the module layer
- platform changes can be absorbed at the protocol adapter layer
- new board variants can reuse the same runtime, workflow, telemetry, and safety structure
- new capabilities can be added with explicit contracts, not ad hoc patches

## 2. Version Governance

This document governs the delivery baseline for future boards.

Versioning rules:

- `major` changes when the architecture boundary changes
- `minor` changes when new capability classes or standard contracts are added
- `patch` changes when wording, examples, or checklists are corrected without changing the architecture

Every future board should record:

- baseline standard version
- platform protocol version
- hardware revision
- enabled capability set

The board should also maintain an explicit experience library for lessons that were learned, corrected, or superseded over time.

Experience governance rule:

- do not silently erase old engineering conclusions
- if a previous conclusion is later found to be wrong, mark it `superseded`
- add the corrected conclusion as a new entry with evidence
- keep both the old and new entries traceable

## 3. Architecture Layers

Firmware must be split into the following layers.

### 3.1 BSP Layer

Location:

- `code/firmware/bsp`

Responsibility:

- UART, ADC, RTC, GPIO, watchdog, flash, LED, voice chip, RS485 direction control
- no business rules
- no platform protocol logic

Rule:

- BSP only exposes stable device-facing functions
- BSP must not know `QR`, `EX`, `SC`, session state, or payment logic

### 3.2 Module Layer

Location:

- `code/firmware/modules`

Responsibility:

- one hardware capability family per module
- examples:
  - pump control
  - valve control
  - electric meter
  - pressure
  - flow
  - card reader

Rule:

- module does not parse platform wire protocol
- module exports only:
  - `init`
  - `tick`
  - `apply_config`
  - `query_state`
  - `query_values`
  - `execute_action`

Module design principles:

- one module owns one real hardware protocol or one tightly-related hardware abstraction
- if a capability changes from relay to RS485 protocol, prefer replacing module implementation instead of rewriting safety or telemetry
- a module may internally use Modbus, DLT645, relay GPIO, pulse counting, or UART framing, but externally should still expose stable business semantics

### 3.3 Runtime / Safety / Workflow Layer

Location:

- `code/firmware/runtime`
- `code/firmware/safety`
- `code/firmware/workflow`

Responsibility:

- state machine
- business session control
- ready checks
- protection and interlocks
- recovery logic
- local card / voice / access workflows

Rule:

- this layer owns business decisions
- this layer must not know AT commands, socket framing, or vendor-specific meter frame layout
- this layer consumes module state and produces business actions

### 3.4 Protocol Layer

Location:

- `code/firmware/protocol`

Responsibility:

- compact wire protocol encode / decode
- request dispatch
- ACK / NK / QS build
- register / heartbeat / snapshot serialization

Rule:

- protocol layer must not directly toggle hardware pins
- protocol layer maps platform intent into runtime or module calls

### 3.5 Transport Layer

Location:

- `code/firmware/net`

Responsibility:

- TCP framing
- reconnect
- send / receive
- modem time sync
- disconnect diagnostics

Rule:

- transport handles bytes and connection state
- business meaning lives above transport

## 4. Fixed Development Contract

Every future board project must freeze these four contracts before broad coding starts.

### 4.1 Wire Contract

Current baseline is the compact envelope:

```json
{"v":1,"t":"SC/QR/EX/RG/HB/SS/ER/AK/NK/QS","i":"imei","m":"msg_id","s":123,"c":"cid","r":"session_ref","p":{}}
```

Rules:

- device only parses agreed fields
- protocol source of truth is:
  - [http://xupengpeng.top/ops/interface-protocols#device-protocols](http://xupengpeng.top/ops/interface-protocols#device-protocols)
- do not keep long-field compatibility in production firmware
- once a board enters production baseline, hidden compatibility branches should be removed from code, not just disabled in docs

### 4.2 Capability Contract

Capability is divided into:

- real feature exposure
- query surface
- action surface
- channel surface

Current real feature codes for this board:

- `pay`
- `cdr`
- `ebr`
- `bkr`

Rule:

- do not expose a feature in `fm` unless it is truly wired, implemented, and intended for platform use
- planned but not enabled abilities belong in documents or config policy, not live feature exposure

### 4.3 Query Contract

Current stable queries:

- `sc=cm,qc=qcs`
- `sc=wf,qc=qwf`
- `sc=cm,qc=qem`

Rule:

- all other queries must return explicit `NK`
- do not let platform infer hidden capabilities from experimental queries

### 4.4 Action Contract

Current stable actions:

- `sc=cm,ac=ppu`
- `sc=wf,ac=pas`
- `sc=wf,ac=res`
- `sc=md,ac=spu,tr=pump_1`
- `sc=md,ac=tpu,tr=pump_1`

Rule:

- unsupported action must return `NK`
- no silent timeout for unsupported commands

## 5. Required File Responsibilities

When adding or changing a capability, engineers must keep these boundaries.

- `proto_dispatch.c`
  - only route allowed inbound message types
- `proto_query.c`
  - only parse allowed query payload keys
  - only expose approved query set
- `proto_execute_action.c`
  - only parse allowed action payload keys
  - only expose approved action set
- `proto_sync_config.c`
  - only parse approved configuration keys
- `proto_register.c`
  - only expose real capability list
- `proto_heartbeat.c`
  - only expose stable health fields
- `proto_state_snapshot.c`
  - only expose approved state values
- `module_xxx.c`
  - own device protocol or hardware interaction
- `safety_flow.c`
  - own business interlock and action admission

## 6. New Capability Expansion Process

Every new capability must follow this order.

### Step 1. Freeze the business name

Decide:

- capability code
- whether it belongs in `fm`
- whether it needs new query code
- whether it needs new action code
- whether it needs new snapshot metric

### Step 2. Freeze the hardware ownership

Decide:

- which module owns it
- which UART / RS485 / GPIO / ADC resource it uses
- whether it conflicts with an existing module

### Step 3. Freeze the platform surface

Decide:

- how platform opens the feature
- exact `QR` form
- exact `EX` form
- exact `NK` behavior on unsupported / invalid / disabled cases

### Step 4. Implement in module first

Rule:

- finish device protocol and local mock / state path first
- only then expose it upward through query / action

### Step 5. Add telemetry

Decide:

- whether the value belongs in `HB`
- whether the value belongs in `SS`
- whether the value belongs in `QS`
- whether failures belong in `ER`

### Step 6. Add acceptance cases

For every new capability, add:

- normal path
- unsupported path
- parameter error path
- hardware disconnected path
- stale or null value path

## 7. Platform Change Rules

Platform requests must be classified before coding.

### Allowed change types

- add a new query
- add a new action
- add a new feature code
- add a new config field
- add a new metric in `SS` or `QS`

### Not allowed without version review

- renaming an existing short code
- changing meaning of an existing short code
- mixing internal platform fields into `wire_message.p`
- forcing device to silently accept deprecated long fields forever

## 8. Module Design Rules

Every module should provide three levels of readiness.

### Level A. Declared

- file scaffold exists
- state model exists
- no real hardware yet

### Level B. Implemented

- local driver works
- values or actions work on hardware
- protocol not yet released to platform

### Level C. Released

- platform query or action is frozen
- debug cases are passed
- telemetry and `NK` behavior are stable

Rule:

- only Level C belongs in real `fm`

## 9. RS485 / Fieldbus Rules

For RS485 devices such as electric meter, breaker, VFD, sensors:

- each protocol family should have one owning module
- line parameters must be explicit:
  - baudrate
  - parity
  - stop bits
  - timeout
  - addressing mode
- one board may support multiple protocol families, but each family must have a clean adapter boundary

Recommended ownership examples:

- `module_meter`
  - owns DLT645 or Modbus meter access
- future `module_breaker_485`
  - owns breaker control and feedback over RS485
- future `module_vfd_485`
  - owns VFD control over RS485

## 10. Platform / Device Separation Rule

Platform internal command model can be rich, but wire protocol must stay minimal.

Rule:

- platform may keep long internal fields like `source`, `target_device_id`, `module_instance_code`
- device should only receive approved compact fields in `wire_message`
- platform internal DTO is not equal to device wire payload

## 11. Required Acceptance Gates

Before marking a capability as delivered, all gates below should pass.

### Gate A. Wire gate

- malformed JSON must be zero
- unknown request must return `NK`
- invalid payload key must return `NK`

### Gate B. Hardware gate

- device interacts with the real peripheral
- unplugged or timeout behavior is visible and bounded

### Gate C. Telemetry gate

- platform receives stable `RG / HB / SS`
- optional fields are either valid values or omitted
- no illegal empty numeric field

### Gate D. Recovery gate

- reconnect still works
- unsupported command still returns `NK`
- boot and disconnect diagnostics remain intact

## 12. Required Documentation Outputs Per Board

Every board or major variant must carry these documents:

- modular development standard baseline
- platform command contract
- debug handoff checklist
- current accepted command list
- hardware protocol notes for each RS485 device

## 13. Standard Delivery Flow

Recommended work order for future boards:

1. freeze compact wire protocol
2. freeze real feature list
3. freeze query and action matrix
4. implement BSP
5. implement modules
6. implement runtime / safety
7. implement protocol adapters
8. integrate platform
9. run debug checklist
10. freeze accepted command text for platform

## 14. Current Board Baseline

Current board baseline under this standard:

- protocol baseline: compact short protocol only
- active feature set:
  - `pay`
  - `cdr`
  - `ebr`
  - `bkr`
- electric meter protocol:
  - `dlt645_2007`
- breaker control:
  - released as the pump supply switching surface
- sensor capability:
  - not released

## 15. Change Log

### v1.0.0

- established modular architecture boundaries
- froze platform / device separation rule
- froze expansion workflow
- froze acceptance gates and documentation set
