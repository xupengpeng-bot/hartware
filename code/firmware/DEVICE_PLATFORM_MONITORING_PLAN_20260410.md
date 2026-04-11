# Device / Platform Monitoring Plan (2026-04-10)

Current firmware version target for this plan: `0.1.4`

## Goal

Use the platform as the source of truth for online/offline duration and use the device as the source of truth for reboot / disconnect cause.

This keeps cost low, keeps the statistics accurate, and still explains why a device went offline.

## Device-side fields

These fields should ride on existing `REGISTER` / `HEARTBEAT` messages instead of adding a new high-frequency message type:

- `boot_session_id`
- `uptime_sec`
- `reset_cause`
- `reset_csr_raw`
- `register_ok_count`
- `heartbeat_ok_count`
- `snapshot_ok_count`
- `network_lost_count`
- `power_loss_count`
- `last_disconnect_reason`
- `last_disconnect_conn_age_ms`
- `last_disconnect_last_tx_type`
- `last_disconnect_last_tx_age_ms`

## Device-side implementation notes

- `boot_session_id` should stay stable across reconnects in the same boot.
- Do not write flash on every disconnect just to count online/offline events.
- Prefer deriving reboot identity from current time sync plus uptime when available.
- Keep disconnect context in RAM and expose the latest one in telemetry.
- Keep heartbeat cadence unchanged and reuse the existing protocol path.

## Platform-side online / offline rules

Assume heartbeat interval is `30s`.

- If no heartbeat is received for `70s`, mark `offline_suspected`.
- If no heartbeat is received for `90s`, mark `offline_confirmed`.
- If any valid upstream message is received after offline, mark `online_recovered`.

## Platform-side tables

### 1. Current status table

Recommended fields:

- `imei`
- `is_online`
- `last_seen_at`
- `last_heartbeat_at`
- `last_snapshot_at`
- `current_boot_session_id`
- `current_uptime_sec`
- `current_reset_cause`
- `firmware_version`
- `hardware_rev`
- `today_register_count`

### 2. Offline event table

Recommended fields:

- `imei`
- `offline_started_at`
- `offline_ended_at`
- `offline_duration_sec`
- `recover_msg_type`
- `recover_boot_session_id`

### 3. Reboot event table

Recommended fields:

- `imei`
- `boot_session_id`
- `booted_at`
- `reset_cause`
- `reset_csr_raw`
- `firmware_version`

## Platform-side daily metrics

- `offline_count`
- `offline_total_sec`
- `availability`
- `register_count`
- `reboot_count`
- `peer_close_suspect_count`

## Platform-side alerts

- More than `3` REGISTERs in `1 hour`: warning
- More than `10` REGISTERs in `1 hour`: critical
- Repeated disconnects with `last_disconnect_reason=modem_closed`: network / server close warning
- Repeated boot session changes in a short period: power / reboot instability warning

## Coordination split

### Device side owns

- Structured reboot cause
- Uptime
- Disconnect cause and latest disconnect context
- Local counters

### Platform side owns

- Offline duration
- Offline rate / availability
- Reconnect storm detection
- Daily / weekly aggregation
- Alerting and dashboards
