# SCAN-IRR-CTRL-4G-A01

Product baseline identity:

- hardware code: `HW-SCAN-IRR-CTRL-4G-A01`
- hardware sku: `SCAN-IRR-CTRL-4G`
- revision: `A01`
- software family: `SCAN-IRRIGATION-CONTROL`
- software version: `v0.2.0`

Product positioning:

- irrigation pump controller with local relay output
- RS485 electric meter access using `DLT645-2007`
- energy metering and billing workflow
- local card reader
- no voice broadcast
- no QR payment module

Baseline source:

- copied from the active root controller program on `2026-04-14`
- source root: [code](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/code)
- flashing helpers: [build_flash](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/build_flash)
- product capability set: [PRODUCT_CAPABILITY_SET_20260414.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/PRODUCT_CAPABILITY_SET_20260414.md)
- board capability inventory: [BOARD_CAPABILITY_INVENTORY_20260414.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/BOARD_CAPABILITY_INVENTORY_20260414.md)

Current rule:

- keep shared framework fixes aligned with the root controller program
- keep this folder as the product branch for `SCAN-IRR-CTRL-4G-A01`
- default control path is local relay direct pump control
- default metering path is `DLT645-2007` over the single `RS485` bus
- charging and settlement use energy snapshots and `meter_epoch`
- configurable runtime fields include `heartbeat_interval_sec`, `overload_protection`,
  `over_current_limit_a`, `phase_loss_protection`, `over_voltage_protection`, `over_voltage_limit_v`

Current scope:

- product identity and version strings have been updated to the new hardware model
- meter, flow and billing safety chain have been restored from the mainline controller baseline
- protocol capability now exposes electric meter query, relay control and billing-related status
- board-level GPIO assumptions have been tightened to one local relay output and one `RS485` meter bus
- this product variant explicitly disables voice broadcast and QR payment exposure
