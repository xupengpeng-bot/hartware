# Embedded OTA Debug Notes 2026-04-14

Updated: `2026-04-14 10:20`

## 1. Remote Log Portal

- Fixed entry: [Remote embedded logs](https://volume-received-disturbed-employ.trycloudflare.com)
- Latest local reachability check: `HTTP 200 OK`
- Check time: `2026-04-14 09:00`

## 2. Usage Rule

- For embedded OTA debugging, check the remote log portal first and then correlate it with local serial captures.
- If this portal becomes unreachable in a future thread, explicitly tell the user in that same thread and record the outage time and impact here.
- Do not silently continue with stale assumptions when the remote log source is unavailable.
- Before any real-device action, first confirm whether the user is currently in `local bench` mode or `remote/log-only` mode.
- If the user confirms `local bench`, default to direct local flashing instead of treating the session as remote-only.

## 2A. Remote Helper Scripts Learned

- The remote portal is not just for logs. It also exposes helper scripts:
  - `build_and_flash.cmd`
  - `flash.cmd`
  - `flash_full.cmd`
  - `flash_gui.cmd`
  - `flash_standalone.cmd`
  - `env_tools.cmd`
  - `erase_chip.cmd`
- From those scripts, the stable flashing rules are:
  - `controller_fw.bin` is written to `0x08004000`
  - standalone/full image is written to `0x08000000`
  - preferred tool order is `STLINK`, optional `STM32CubeProgrammer CLI`, then `OPENOCD`, then `SERIAL`

## 2B. Local Flashing Toolchain Learned

- Confirmed local flashing executable:
  - `D:\Tools\STLink\bin\st-flash.exe`
- Confirmed local wrapper entry:
  - `D:\Develop\houji\houjinongfuAI-Cursor\hartware\build_flash\flash.cmd`
- Confirmed local helper assets under:
  - `D:\Develop\houji\houjinongfuAI-Cursor\hartware\build_flash\tools`
- Operational rule for future local sessions:
  - once the user confirms the board is locally connected, Codex may directly use the local flashing wrapper/toolchain without asking for a second flash-specific permission

## 3. Confirmed OTA Facts For This Round

- After slimming, the OTA package size `115608 bytes` is no longer a staging-partition overflow issue.
- `upg` command delivery, device `AK`, and platform `acked` bookkeeping are already working.
- The remaining failures moved from `waiting_ack` and `content_length_mismatch` to end-of-download integrity failures.
- When serial logs are built in slim mode and no longer print detailed `HTTP/1.1 200/206` traces, use platform upgrade-job details as the source of truth for the final failure code.

## 4. Proven Investigation Order

1. Check whether the platform job reached `acked` to rule out a delivery problem.
2. Check whether the device actually entered `ota_pause` and started the HTTP download session.
3. If the failure is `content_length_mismatch`, compare the real server `Range` response before changing package generation.
4. If the failure is `sha256_mismatch` at `100%`, inspect the device-side downloaded byte stream before blaming flash write.
5. Only after download integrity is proven should later-stage issues such as `flash_write_failed` or missing `boot_confirmed` be treated as primary.

## 5. Landed Fixes Before Final Closure

- Hardened HTTP header parsing in [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c)
- Relaxed recoverable `206 Partial Content` header handling while keeping strict `ETag`, manifest size, final byte count, and `SHA256`
- Fixed platform/device ACK bookkeeping for OTA command paths
- Reduced OTA build size so the package remains within the staging slot budget

## 6. 2026-04-14 Final Root Cause Closure

- Confirmed root cause: the remaining `sha256_mismatch` was caused by the device-side OTA HTTP transport mutating the delivered byte stream with odd-byte alignment logic before the bytes reached `proto_ota` SHA256.
- Why it failed: [proto_ota.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/protocol/proto_ota.c) hashes the downloaded bytes before flash write, so any byte deferral or reordering in [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c) can produce a size-correct but hash-wrong stream.
- Safety confirmation: [stm32f1_flash.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/platform/stm32f1_flash.c) already supports odd-length writes by padding the final halfword with `0xFF`, so HTTP-layer alignment is unnecessary.
- Final rule: OTA HTTP body delivery must stay byte-exact; flash alignment belongs only in the flash writer, not in HTTP/body assembly.
- Landed fix:
  - removed odd-byte deferral from [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c)
  - kept spill buffering only for real overflow beyond the caller buffer
  - bumped runtime-reported firmware version to `0.1.29`

## 7. 2026-04-14 Late Flash-Write Follow-Up

- New observed stage after the `0.1.29` transport fix: OTA could reach `AK` and deep download progress but then fail near `99%` with `flash_write_failed`.
- Key correction: this was no longer a package-size issue. The staging slot is `118784 bytes`, while the tested `0.1.29` OTA package was `115504 bytes`.
- Most likely device-side mechanism: many direct `256-byte` streaming writes into STM32F103 staging flash are more fragile near the tail than page-buffered writes, especially after multiple resume cycles and late-session retries.
- Landed safety fix:
  - staging writes are now buffered by `2 KiB` flash page in [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c)
  - each page is written once and read back for verification
  - if a page write fails, that page is erased and retried once before the OTA is failed
  - the final partial page is flushed explicitly before reboot scheduling
- Current runtime version after this fix: `0.1.30`

## 8. 2026-04-14 HTTP Single-Field QIRD Follow-Up

- New observed contradiction: the device was already running `0.1.29`, which included the byte-exact HTTP transport fix, yet the `0.1.30` OTA still failed at `100%` with `sha256_mismatch`.
- Confirmed narrowing: platform TCP audits showed delivery and `AK` were correct, and the failure still happened before any `0.1.30` runtime was active, so the remaining corruption had to stay inside the running modem receive path.
- Most likely mechanism: during OTA HTTP, EC801 sometimes returns the single-field `+QIRD: <n>` form. If the device does not immediately probe `AT+QIRD=0,0` after that fetch, a hidden unread tail can be dropped even though the total download size still looks correct.
- Landed fix:
  - after every successful single-field `QIRD` fetch, the modem now performs one authoritative unread probe
  - the returned unread field is used as the true pending tail for the next HTTP fetch
  - runtime-reported firmware version is bumped to `0.1.31`

## 9. 2026-04-14 Legacy Strong ETag Compatibility

- New observed stage after the `0.1.31` downloader fix attempt: the field device, still running `0.1.29`, could reject the OTA immediately at `0%` with `etag_mismatch`.
- Confirmed narrowing: the platform dispatch payload had started sending `etag` as a bare SHA256 string, while the artifact download endpoint still returned the HTTP response header in standard strong form like `"hash"`.
- Most likely mechanism: the legacy field downloader compares the manifest `etag` against the HTTP response `ETag` in canonical quoted strong form; stripping the quotes in backend dispatch breaks compatibility before download progress even starts.
- Landed fix:
  - backend OTA dispatch now preserves canonical strong `ETag` quotes in the wire payload
  - local embedded downloader remains tolerant of both quoted and bare forms
  - this keeps `0.1.29` field devices and newer downloaders compatible with the same release metadata

## 10. 2026-04-14 Fixed-Downloader Bridge Rule

- New observed stage after restoring quoted strong `ETag`: the same `0.1.31` package stopped failing at `0%` with `etag_mismatch`, but the field device still running `0.1.29` continued to fail at `100%` with `sha256_mismatch`.
- Confirmed narrowing: backend dispatch, ACK bookkeeping, package size, and quoted `ETag` were all correct; the remaining corruption still came from the legacy `0.1.29` downloader path rather than from release metadata.
- Latest confirmation from `gui_serial_COM4_20260414_102754`: one capture window contained two separate `AK`-accepted OTA attempts and one replay of the same command token; both real attempts entered `ota_pause`, later reconnected, and still registered `fv":"0.1.29"`.
- Secondary finding from the same trace: long `downlink idle`, `peer_closed`, and `modem_closed` periods can trigger replay noise and confuse platform accounting, but they did not change the primary fact that no newer downloader runtime ever started.
- Final operational rule: once release metadata is correct, repeated OTA retries cannot prove the fixed downloader until that downloader is running on the device. The bridge must be:
  - manually seed the device to the fixed downloader build (`0.1.32`)
  - then validate the downloader with a newer OTA target (`0.1.33`)
- Landed action:
- reserved runtime version `0.1.32` as the current fixed-downloader bridge build
- next OTA validation target must now be newer than `0.1.32`

## 2026-04-14 11:22 CST - Mixed-version BIN caused by stale incremental object

- New observed contradiction: the user reported manually flashing the latest build, but the device still registered `fv":"0.1.29"` while the app boot banner path had already moved to `0.1.32`.
- Root cause: the previous incremental build produced a mixed-version BIN. `controller_fw.bin` contained both `0.1.29` and `0.1.32`; specifically, `common_identity.c.obj` was stale and still embedded `0.1.29`, while `main.c.obj` had already embedded `0.1.32`.
- Evidence:
  - `common_identity.c` is the runtime source that copies `SCAN_TRIAL_SOFTWARE_VERSION` into `identity.firmware_version`, which then feeds register/OTA event payloads.
  - binary inspection showed the stale object in `libcontroller_firmware.a`, while the executable startup object carried the new version string.
- Fix:
  - added an explicit build dependency from `firmware/common/common_identity.c` and `firmware/app/main.c` to `firmware/config/scan_trial_defs.h` in `code/CMakeLists.txt`
  - performed a full clean rebuild instead of trusting incremental compile
- Result:
  - rebuilt `controller_fw.bin` now contains only `0.1.32`
  - future version-header bumps must use a clean rebuild, or at minimum verify the final BIN does not contain mixed version strings before release

## 2026-04-14 11:45 CST - Retried OTA reused modem socket without a fresh ota_pause

- New observed contradiction from [tmp_gui_serial_COM4_20260414_112540.remote.txt](/D:/Develop/houji/houjinongfuAI-Cursor/tmp_gui_serial_COM4_20260414_112540.remote.txt): the first `0.1.32 -> 0.1.33` attempt still showed the expected `AK -> ota_pause` transition, but the second accepted OTA in the same runtime emitted `ER/upg accepted` and then immediately flooded `[PROTO] RX invalid prefix ... shift=1 for resync` without a second `ota_pause`.
- Hard narrowing:
  - the local OTA SHA256 implementation matches standard SHA256 on boundary vectors and on `_verify_0_1_33.bin`, so the hash core itself is not the source of corruption
  - the invalid-prefix storm grows in `256-byte` chunks, which matches raw modem TCP body polling rather than framed business JSON
- Root cause in code:
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c) tracked OTA control ownership with the port-local flag `s_http.paused_network`
  - OTA terminal callbacks in [app_main.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/app/app_main.c) resumed connectivity through `net_connectivity_resume_after_ota()` after failure/success, but that path did not clear `s_http.paused_network`
  - on the next OTA attempt, `ota_http_pause_network()` trusted the stale flag and skipped the real `net_connectivity_pause_for_ota()` handoff, so the control socket remained logically connected while the modem was repurposed to HTTP port `80`
  - [net_socket_client.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_socket_client.c) then kept reading the modem FIFO as if it were business frames and treated HTTP response bytes as bogus 4-byte JSON length prefixes
- Landed fix:
  - `ota_http_pause_network()` now always delegates to `net_connectivity_pause_for_ota()`
  - `ota_http_resume_network()` now always delegates to `net_connectivity_resume_after_ota()`
  - `s_http.paused_network` is kept only as OTA-port bookkeeping, not as the source of truth for control-session ownership
- Expected verification:
  - after an OTA terminal failure, the next accepted OTA attempt must log a fresh `ota_pause`
  - no new `RX invalid prefix ... shift=1 for resync` storm should appear immediately after `ER/upg accepted`

## 2026-04-14 11:52 CST - OTA HTTP must own the modem receive path exclusively

- New observed contradiction: after the bridge device was truly running `0.1.32`, `0.1.33` still failed at `100%` with `sha256_mismatch`, even though package upload, quoted strong `ETag`, ACK bookkeeping, and final package size were all correct.
- Root cause narrowing:
  - [proto_ota.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/protocol/proto_ota.c) hashes bytes before flash write, so `flash_write_failed` and staging flash alignment cannot explain this checksum failure.
  - [net_connectivity.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_connectivity.c) was still calling `net_4g_modem_poll()` even while `s_paused_for_ota != 0`, which let the generic business-TCP `QIRD pending/probe/FIFO` machine keep touching the same HTTP socket between OTA chunk reads.
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c) was also reading OTA payload through the shared modem TCP FIFO via `net_4g_modem_tcp_rx_pop()`, so OTA body bytes could still be reordered or dirtied by shared receive-path state even when total length stayed correct.
- Landed fix:
  - OTA HTTP now bypasses the shared TCP FIFO and reads payload directly from `AT+QIRD` into the HTTP parser through the new direct modem fetch path in [net_4g_modem.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_4g_modem.c)
  - `net_connectivity_poll()` now returns before `net_4g_modem_poll()` when paused for OTA, so business connectivity no longer background-polls the modem during an active HTTP download
  - runtime seed version bumped to `0.1.34`
- Operational rule:
  - when OTA is active, HTTP download must exclusively own modem receive state
  - business-TCP polling, FIFO draining, and OTA HTTP body delivery must not share the same live `QIRD` state machine

## 2026-04-14 12:08 CST - OTA pause must leave a flush window after accepted ACK/event

- New observed contradiction from [gui_serial_COM4_20260414_115538.txt](https://volume-received-disturbed-employ.trycloudflare.com/gui_serial_COM4_20260414_115538.txt):
  - runtime `0.1.34` really accepted the bridge OTA command and emitted local `AK` plus `ER/upg accepted`
  - the same capture then logged `disconnect ctx reason=ota_pause ... last_tx=ER/378B 0 ms ago`
  - backend job `9489958a-3395-4467-b829-a1abb259f5a9` still stayed at `command_sent_waiting_ack`, which means the platform never actually received those just-emitted `AK/ER` frames
- Root cause in code:
  - [app_main.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/app/app_main.c) sends `OTA_EVENT_OTA_COMMAND_ACKED` as an `ER/upg accepted` report immediately before the downloader pauses the control socket
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c) then asks [net_connectivity.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_connectivity.c) to `ota_pause`
  - before this fix, `net_connectivity_pause_for_ota()` disconnected the business TCP session immediately, even if the last uplink frame had been sent `0 ms` earlier
- Landed fix:
  - `net_connectivity_pause_for_ota()` now applies an `800 ms` flush guard when the latest uplink frame is still fresh, then disconnects for OTA handoff
  - the guard logs `[PROTO] ota_pause flush guard ...` so the next validation can confirm the new timing path was used
- Expected verification:
  - on the next accepted OTA attempt, serial should show the flush-guard log before `ota_pause`
  - platform should finally move the job out of `command_sent_waiting_ack` as soon as the device emits `AK/ER accepted`

## 2026-04-14 12:24 CST - ETag becomes advisory once package SHA256 is mandatory

- New observed contradiction from [gui_serial_COM4_20260414_121141.txt](https://volume-received-disturbed-employ.trycloudflare.com/gui_serial_COM4_20260414_121141.txt):
  - runtime `0.1.35` accepted the OTA command and progressed to `85%`
  - backend job `cc93d9f9-d512-4b4c-a1c9-3cd8c3c06584` then failed with `etag_mismatch`
  - direct checks against the artifact URL showed both `200 OK` and `206 Partial Content` returning the same quoted strong `ETag`, correct `Content-Range`, and correct `Content-Length`
- Root cause in code:
  - [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c) still treated response `ETag` as a hard gate during resumed HTTP downloads
  - any parser edge case, header variation, or missing `ETag` on a retry path could abort OTA early with `etag_mismatch`, even though the downloader already had the stronger final `SHA256` integrity check in [proto_ota.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/protocol/proto_ota.c)
- Landed fix:
  - manifest `package_etag` normalization no longer aborts the session; it falls back to advisory mode
  - response `ETag` parse/mismatch on `200/206` now logs an advisory bypass instead of hard-failing the OTA session
  - final package integrity still depends on mandatory package size plus final `SHA256` verification
- Safety rule:
  - when package SHA256 is mandatory, `ETag` is an optimization and diagnostic hint, not the final integrity authority
  - do not let resumed `206` header variance override the stronger end-to-end digest check

## 2026-04-14 16:27 CST - Local bench `0.1.40 -> 0.1.41` OTA still fails with staged-byte corruption

- Mode confirmation:
  - this run was `local bench`, not remote-only
  - live serial evidence came from [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt)
- Exact release under test:
  - source version in [scan_trial_defs.h](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/config/scan_trial_defs.h) is `0.1.41`
  - local OTA artifact was [controller_fw.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_codex_arm/controller_fw.bin)
  - artifact size was `118536 bytes`
  - artifact SHA256 was `ce6a9cfe5ff2195614643d9d88c16813f025511ff9eec04d08f8717b13b3d733`
- Dispatch path and platform result:
  - OTA was dispatched directly through `npx ts-node scripts/ota-upgrade-device.ts --imei 861295087573980 --bin ...`
  - release id: `7751c899-107b-4f3e-8096-790b741cbf28`
  - job id: `18561fda-bdb0-4949-a7ed-8e62ac8f74bc`
  - target version: `0.1.41-r20260414162737`
  - final platform result: `accepted -> failed@100% -> sha256_mismatch`
- Local serial correlation:
  - device was running `0.1.40` before this OTA attempt at [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt:11416)
  - command reached `AK` / `accepted` at [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt:11882)
  - the device entered the expected flush-guard + `ota_pause` handoff at [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt:11887) and [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt:11888)
  - after the run, the device re-registered still as `0.1.40` at [local_serial_COM3_20260414_1313.txt](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/local_serial_COM3_20260414_1313.txt:13183)
  - no `0.1.41` boot banner or register line appeared in the same capture window
- New hard evidence from local staging dump:
  - after the failed OTA, the staging slot was read back from `0x08022000` size `118784` into [staging_dump_20260414_ota41_failed.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/staging_dump_20260414_ota41_failed.bin)
  - comparing the first `118536` staging bytes against the exact release artifact showed:
    - staged prefix SHA256: `2507ae8d7b867d0b35c8efde6a241fdfdfda7e3a043b9ac17bc2dccb2d590e68`
    - differing bytes: `8717`
    - differing segments: `195`
    - first differing offset: `2149`
- Current narrowing:
  - this is no longer just a platform-side `sha256_mismatch` report; the staged image bytes on the board are measurably different from the release BIN
  - therefore the primary fault remains in the pre-boot content path: OTA HTTP body assembly, staged write path, or their interaction
  - boot-confirm logic, post-reboot registration, and ACK bookkeeping are not the primary cause of this specific run
- New operational rule:
  - whenever a local bench OTA ends with `100% sha256_mismatch`, preserve three artifacts in the same round:
    - exact release BIN + SHA256
    - matching local serial window
    - staging dump readback from the board
  - do the release-vs-staging byte compare before proposing new transport redesigns or blaming the platform

## 2026-04-14 16:50-17:05 CST - Follow-up fixes and non-fixes after the local staging-dump proof

- A targeted hardening was landed in [bsp_uart.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/bsp/bsp_uart.c):
  - `BOARD_HW_UART_PORT_MODEM_4G` readout now makes the `UART4` FIFO/`DR` choice inside one IRQ-masked critical section
  - reason: the previous `fifo_pop -> snapshot SR -> direct DR` path had a real race where `UART4_IRQHandler()` could consume the same byte between the stale `RXNE` snapshot and the direct `DR` read
  - this race matches the observed staging-dump signature of repeated "insert previous byte" corruption, so it is worth keeping even though it did not close the whole OTA failure
- Bench result after that hardening:
  - local OTA still reproduced `accepted -> failed@100% -> sha256_mismatch`
  - representative run used [controller_fw.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_codex_arm/controller_fw.bin) with SHA256 `6e0e8d4f7be521284442812dc79bb691fca60206e458b82e479c29d2bcf73714`
  - backend job: `280806e8-33e2-42ba-9f87-bd2a52dba77a`
- A tested non-fix was also confirmed and reverted:
  - changing [ota_port_board.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/port/ota_port_board.c) so each `http_download_chunk()` returned immediately after any assembled body bytes caused a much worse bench result
  - the reverted variant produced a staging diff of `114805` bytes across `9` very large segments, so "stop cross-fetch coalescing" is not a safe fix by itself on the current downloader
- A new debug-build trap was observed:
  - adding always-on OTA boundary diagnostics increased `controller_fw.bin` to `118928 bytes`
  - the staging slot is only `118784 bytes`, so the next OTA command was rejected locally with `NK rc=BZ msg=storage low`
  - this rejection is a packaging/slot-capacity issue, not a transport-integrity verdict

## 2026-04-14 17:05-17:20 CST - HTTP partial-line residue cleanup helps, but aggressive UART absorb regresses badly

- A second downloader hardening was tested in [net_4g_modem.c](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/code/firmware/net/net_4g_modem.c):
  - add `modem_http_discard_partial_line_residue()` to discard only incomplete shared-stream tail bytes during HTTP QIRD work
  - call it around direct HTTP fetch seams, and also from `modem_prepare_at_rx()` while the HTTP download session owns the modem socket
  - purpose: prevent a stale trailing control/payload byte from leaking into the next `QIRD` seam and repeating the previous byte inside the staged image
- Measured effect of the conservative shared-stream-only cleanup:
  - representative bench run artifact SHA256: `c0ab93ea13469ccda4580838f17bca773b4d6fc9c635cd551e23f4889f7faa5b`
  - backend job: `91ad1265-a43d-4108-80a7-816416a8cf23`
  - platform still ended `accepted -> failed@100% -> sha256_mismatch`
  - but the post-failure staging dump [staging_dump_20260414_ota41_failed_residuefix.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/staging_dump_20260414_ota41_failed_residuefix.bin) improved materially versus the prior UART-only fix:
    - differing bytes dropped from `8944` to `6486`
    - differing segments dropped from `142` to `123`
    - first diff moved earlier to `904`, but every diff-segment start still matched the "repeat previous byte at a fetch seam" pattern
- A stronger variant was also tested and reverted in the same round:
  - extending that helper so it first called `stream_push_from_uart()` and then discarded the residue caused a much worse staged image
  - artifact SHA256 in that failed experiment: `d9f82680e60e6b40d03597ce101573600ed132a0a030bce13ba16cab859aad4d`
  - the readback [staging_dump_20260414_ota41_failed_residuefix2.bin](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/build_flash/logs/staging_dump_20260414_ota41_failed_residuefix2.bin) differed in `57974` bytes across `4470` segments, with single-byte corruption beginning at offset `404`
  - conclusion: cleaning partial shared-stream residue is useful, but proactively absorbing more UART bytes into that cleanup can destroy valid in-flight modem data and is not safe
- Platform bookkeeping caveat observed in the same window:
  - backend job `10772478-3f17-4e34-aeee-ef9c27b31ef9` stayed at `command_sent` in the polling script, but the live serial log still showed a real OTA run for token `ab0dba1f-67f8-424c-832e-055e6362772e`, followed by re-registration still at `fv":"0.1.40"`
  - for bench closure, device serial plus staging dump remained the source of truth; backend polling alone was not reliable enough for this round
