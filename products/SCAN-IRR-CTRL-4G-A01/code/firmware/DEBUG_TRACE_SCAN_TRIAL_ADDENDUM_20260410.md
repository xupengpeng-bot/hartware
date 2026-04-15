# 2026-04-10 Addendum

This addendum records the extra fixes made after the earlier scan-trial trace note.

## 1. Downlink first-frame loss

- Symptom:
  - Platform downlink `QUERY` existed in TCP audit, but device often did not route it to `safety_flow`.
- Root cause:
  - `net_socket_client_poll()` fed inbound modem chunks into `net_socket_client_feed()`, but ignored the return value while draining the chunk loop.
  - If a full frame was completed during that loop, it could be extracted and then effectively dropped before the caller observed it.
- Fix:
  - Return immediately when `net_socket_client_feed()` reports one full frame.
- File:
  - `code/firmware/net/net_socket_client.c`

## 2. Repeated-card busy window hardening

- Symptom:
  - Under repeated swipes, logs showed `guard=0` followed by `dispatch ... rc=-3`.
  - UX also showed false `welcome` even though the card was actually rejected as busy.
- Root cause:
  - Guard logic did not explicitly reject card flow while `card_state` was still busy or while workflow was already in `AUTH_PENDING`.
  - `welcome` prompt was emitted before `safety_flow_on_card_read()` returned success.
- Fix:
  - Guard now rejects when card flow is not idle/completed/blocked.
  - Guard also rejects when workflow is already `AUTH_PENDING`.
  - `welcome` now plays only after a successful card accept.
  - Busy rejection `rc=-3` now maps to `port_busy`, not `unavailable`.
- File:
  - `code/firmware/workflow/workflow_card_reader.c`

## 3. Busy swipe no longer corrupts card state

- Symptom:
  - Repeated swipes during an in-flight auth could produce `rc=-3`.
- Root cause:
  - `safety_flow_on_card_read()` returned `BUSY`, but also forced card state to `CARD_STATE_BLOCKED`.
  - That was unsafe because it could clobber an in-progress auth state.
- Fix:
  - Busy path now returns immediately without changing the current card state.
- File:
  - `code/firmware/safety/safety_flow.c`

## 4. Boot reset-cause tracing

- Symptom:
  - Serial logs showed repeated `=== APP main() entry ===`, but the reset source was unknown.
- Fix:
  - Added boot-time `RCC_CSR` reset-cause log.
  - New log format:
    - `[BOOT] reset_csr=0x........ cause=PIN|POR|SOFT|IWDG|WWDG|LPWR`
- Purpose:
  - Distinguish external reset, power reset, software reset, and watchdog reset during future reboot captures.
- File:
  - `code/firmware/app/main.c`

## 5. Build status

- Rebuilt successfully after the above fixes.
- Output binaries:
  - `C:\Users\xupen\AppData\Local\hw_embedded_build\out\build\controller_fw.bin`
  - `C:\Users\xupen\AppData\Local\hw_embedded_build\out\build\controller_fw_standalone.bin`

## 6. Snapshot malformed-json hardening

- Symptom:
  - Platform still observed malformed `STATE_SNAPSHOT`, including payload fragments like:
    - `"voltage_v":,"current_a":,"power_kw":...`
- Root cause:
  - Historical builds still had a float/empty-string formatting path that could leave numeric fields blank.
  - Even after moving to fixed-point formatting, there was no final self-check preventing malformed numeric gaps from being sent.
- Fix:
  - Added numeric sanitization before snapshot field emission.
  - Added a final malformed-fragment guard that rejects any built snapshot containing `":,`.
  - Invalid or implausible numeric inputs now fall back to safe defaults instead of producing an empty field.
- Files:
  - `code/firmware/protocol/proto_codec_json.c`
  - `code/firmware/protocol/proto_state_snapshot.c`

## 7. Full snapshot JSON logging

- Symptom:
  - Serial logs only showed a truncated JSON preview, which made it hard to compare exact snapshot payloads against platform-side malformed samples.
- Fix:
  - `STATE_SNAPSHOT` now emits a chunked full JSON log before send.
  - New log format:
    - `[JSON-FULL] STATE_SNAPSHOT part=0 ...`
    - `[JSON-FULL] STATE_SNAPSHOT part=1 ...`
- File:
  - `code/firmware/app/app_scheduler.c`

## 8. QIRD failure no longer forces immediate reconnect

- Symptom:
  - A single `QIRD` receive failure could drop TCP immediately, causing rapid reconnect and repeated `REGISTER`.
- Fix:
  - Added `QIRD` failure streak tracking.
  - One or two failed receive attempts now keep TCP alive and wait for the next URC / modem state update.
  - TCP is only marked disconnected after repeated receive failures.
- File:
  - `code/firmware/net/net_4g_modem.c`

## 9. Reconnect backoff widened

- Symptom:
  - Reconnect cadence was too aggressive, amplifying `REGISTER` churn when the modem or upstream link flapped.
- Fix:
  - Increased reconnect backoff from `5000 ms` to `15000 ms`.
- File:
  - `code/firmware/net/net_connectivity.c`

## 10. Firmware versioning convention

- Convention:
  - Every code-change round in this scan-trial debug thread must bump a small firmware version.
  - The version is the primary way to correlate:
    - serial boot logs
    - `REGISTER.software_version`
    - platform-side ingest / audit observations
- Rule:
  - Use monotonic patch-version increments such as:
    - `0.1.1`
    - `0.1.2`
    - `0.1.3`
    - `0.1.4`
  - After each change round, the closing note to the user must explicitly state the current firmware version.
- Current version for this round:
  - `0.1.4`
- Visibility:
  - Version is carried in `REGISTER.software_version`.
  - Boot log now prints:
    - `[BOOT] firmware_version=0.1.4`

## 11. QIRD partial-drain and QISEND stall hardening

- Symptom:
  - Device could receive a partial downlink frame, log `QIRD partial payload read=...`, then soon enter repeated `QISEND no prompt`.
  - After that, outbound `QUERY` and `HEARTBEAT` sends failed, but the modem/session could linger in a half-dead state instead of reconnecting cleanly.
- Root cause:
  - A partial `QIRD` read kept TCP alive, but the unread tail was not guaranteed to be drained on subsequent polls.
  - Raw partial chunks were also logged as if each chunk were a standalone framed JSON packet, which obscured the true failure mode.
  - `QISEND no prompt` meant the modem data plane was already unhealthy and should be treated as a broken TCP session.
- Fix:
  - Track pending unread `QIRD` bytes across polls and continue draining them until complete.
  - Log partial chunks explicitly as partial chunks instead of fake complete frames.
  - Treat `QISEND no prompt` as a hard transport failure and mark TCP disconnected immediately so reconnect can proceed.
- File:
  - `code/firmware/net/net_4g_modem.c`

## 12. Post-register send pacing and disconnect context

- Symptom:
  - Right after reconnect, device could send `REGISTER` successfully, then immediately send `HEARTBEAT + STATE_SNAPSHOT` in the same registration window and occasionally hit `QISEND no prompt`.
  - Long logs also showed repeated reconnects, but lacked enough context to tell whether the close happened during idle, right after a specific uplink, or during a workflow transition.
- Root cause:
  - Scheduler treated `REGISTER` send success as the moment to immediately burst two more frames onto a fresh TCP session.
  - Disconnect logs did not include connection age, last uplink type, or current workflow state, which made peer-side close diagnosis too opaque.
- Fix:
  - Added a post-register pacing window:
    - initial `HEARTBEAT` delayed by `1500 ms`
    - initial `STATE_SNAPSHOT` delayed by `3000 ms`
  - If the delayed first heartbeat send fails, the first snapshot is no longer forced in the same tick.
  - Added disconnect context logging with:
    - disconnect reason
    - connection age
    - last uplink type / length / idle age
    - workflow state
    - run state
    - ready flag
  - Added full `REGISTER` JSON chunk logging for easier platform-side field verification.
- Files:
  - `code/firmware/app/app_scheduler.c`
  - `code/firmware/net/net_connectivity.c`

## 13. Device / Platform monitoring alignment (v0.1.4)

- Added structured boot diagnostics for telemetry:
  - `boot_session_id`
  - `uptime_sec`
  - `reset_cause`
  - `reset_csr_raw`
- Added structured disconnect diagnostics for telemetry:
  - `last_disconnect_reason`
  - `last_disconnect_conn_age_ms`
  - `last_disconnect_last_tx_type`
  - `last_disconnect_last_tx_age_ms`
- Added runtime counters to `REGISTER` / `HEARTBEAT` for low-cost health monitoring:
  - `register_ok_count`
  - `heartbeat_ok_count`
  - `snapshot_ok_count`
  - `network_lost_count`
  - `power_loss_count`
- Added coordination document:
  - `DEVICE_PLATFORM_MONITORING_PLAN_20260410.md`

## 14. Battery voltage telemetry exposed (v0.1.5)

- Symptom:
  - Battery voltage was sampled into `common_status`, but platform-visible telemetry only exposed `battery_soc`.
- Root cause:
  - `HEARTBEAT` and `query_common_status` did not serialize `battery_voltage_v` or `solar_voltage_v`.
- Fix:
  - Added `battery_voltage_v` and `solar_voltage_v` to `HEARTBEAT`.
  - Added `battery_voltage_v` and `solar_voltage_v` to `query_common_status`.
- Files:
  - `code/firmware/protocol/proto_heartbeat.c`
  - `code/firmware/protocol/proto_query.c`

## 15. Heartbeat battery voltage JSON hardening (v0.1.6)

- Symptom:
  - `HEARTBEAT build ok` was followed by `outbound JSON validation failed`.
- Root cause:
  - `HEARTBEAT` serialized battery voltages through `%f` formatting on the embedded printf path, which could leave invalid numeric output on target.
- Fix:
  - Switched `battery_voltage_v` and `solar_voltage_v` to the same safe fixed-point JSON path used by `STATE_SNAPSHOT`.
  - Added full `HEARTBEAT` JSON chunk logging for direct serial verification.
- Files:
  - `code/firmware/protocol/proto_heartbeat.c`
  - `code/firmware/app/app_scheduler.c`

## 16. Battery SOC recalibrated for 8.4V full scale (v0.1.7)

- Symptom:
  - Battery voltage telemetry matched the real pack voltage around `8.3V ~ 8.4V`, but `battery_soc` remained near `0%`.
- Root cause:
  - SOC was still using the old high-voltage threshold range (`10.5V` empty to `16.8V` full), which did not match the deployed 2S battery pack.
- Fix:
  - Recalibrated SOC mapping to the confirmed pack range:
    - `8.4V = 100%`
    - `6.0V = 0%`
  - Kept battery voltage telemetry unchanged; only the percentage mapping was adjusted.
- Files:
  - `code/firmware/bsp/bsp_adc.c`

## 17. Battery SOC empty threshold aligned to 7.2V (v0.1.8)

- Symptom:
  - The temporary SOC remap used a wider 2S range than the actual deployed pack policy.
- Root cause:
  - Only the full-scale point (`8.4V`) had been confirmed initially, so the empty threshold was set conservatively for first-pass bring-up.
- Fix:
  - Updated the final deployed SOC calibration to:
    - `8.4V = 100%`
    - `7.2V = 0%`
  - Voltage telemetry remains unchanged; only the percentage mapping was tightened to the confirmed pack window.
- Files:
  - `code/firmware/bsp/bsp_adc.c`

## 18. QIRD pending no-data loop guard (v0.1.9)

- Symptom:
  - Serial logs could get stuck in a high-frequency loop like:
    - `AT+QIRD=0,406`
    - `QIRD pending fetch no data remain=406`
  - Once entered, the modem poll path retried forever and flooded the log.
- Root cause:
  - The pending-QIRD state machine treated `fetch_rc == 0` as a harmless no-op.
  - `s_qird_pending_len` was left unchanged, no retry backoff was applied, and no terminal condition existed for repeated zero-byte fetches.
- Fix:
  - Added pending-QIRD retry backoff (`500 ms`) to avoid per-poll AT spam.
  - Added a separate no-data streak counter for the pending-QIRD path.
  - If pending-QIRD returns zero bytes repeatedly, the code now marks the TCP session disconnected and clears stale pending state so reconnect can recover cleanly.
  - Also reset pending-QIRD state whenever TCP is closed or a send path declares the socket unhealthy.
- Files:
  - `code/firmware/net/net_4g_modem.c`

## 19. Null-safe telemetry serialization and register backoff (v0.1.10)

- Symptom:
  - Numeric telemetry fields could be forced to placeholder defaults instead of explicitly indicating missing data.
  - `REGISTER` retry cadence remained too rigid when send/build failures repeated.
- Root cause:
  - `STATE_SNAPSHOT` and related query surfaces still used fallback numbers for invalid metrics.
  - `REGISTER` retry used a fixed interval instead of staged backoff.
- Fix:
  - Added null-safe numeric field helpers for JSON serialization.
  - `STATE_SNAPSHOT` now emits `null` for unavailable meter/flow metrics instead of placeholder values.
  - `HEARTBEAT` and `query_common_status` now keep fixed field names and emit `null` when battery/signal values are unavailable.
  - `query_meter_snapshot` now emits `null` for unavailable numeric meter fields.
  - `REGISTER` now always uses fixed version / hardware fields from the scan trial definitions.
  - Added staged register retry backoff with jitter:
    - `2s / 4s / 8s / 16s / 30s + jitter`
- Files:
  - `code/firmware/protocol/proto_codec_json.c`
  - `code/firmware/protocol/proto_codec_json.h`
  - `code/firmware/protocol/proto_heartbeat.c`
  - `code/firmware/protocol/proto_state_snapshot.c`
  - `code/firmware/protocol/proto_query.c`
  - `code/firmware/protocol/proto_register.c`
  - `code/firmware/net/net_connectivity.c`
