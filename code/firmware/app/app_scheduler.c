#include "app_scheduler.h"

#include "app_context.h"
#include "common_status.h"
#include "config_store.h"
#include "module_flow.h"
#include "module_meter.h"
#include "module_pump_vfd.h"
#include "module_registry.h"
#include "net_connectivity.h"
#include "runtime_state.h"
#include "safety_flow.h"
#include "telemetry.h"
#include "workflow_card_reader.h"
#include "workflow_voice.h"

#include "bsp_uart.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint32_t s_last_100ms;
static uint32_t s_last_1s;
static uint32_t s_last_30s;
static uint32_t s_last_heartbeat_ms;
static uint32_t s_last_snapshot_ms;
static uint32_t s_registered_window_open_ms;
static uint8_t  s_registered_window_open;
static uint8_t  s_prime_heartbeat_pending;
static uint8_t  s_prime_snapshot_pending;
static char s_json_buffer[2048];

#define SCHEDULER_REGISTER_PRIME_HEARTBEAT_DELAY_MS 1500U
#define SCHEDULER_REGISTER_PRIME_SNAPSHOT_DELAY_MS  3000U

static void scheduler_log_full_json(const char *type, const char *json, size_t len)
{
    char line[224];
    size_t offset = 0U;

    if (type == NULL || json == NULL || len == 0U) {
        return;
    }

    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > 160U) {
            chunk = 160U;
        }
        (void)snprintf(line, sizeof(line), "[JSON-FULL] %s part=%lu %.*s\r\n",
                       type,
                       (unsigned long)(offset / 160U),
                       (int)chunk,
                       json + offset);
        bsp_debug_log(line);
        offset += chunk;
    }
}

static void scheduler_log_send(const char *type, int rc)
{
    char line[96];

    (void)snprintf(line, sizeof(line), "[PROTO] %s %s\r\n", type, rc >= 0 ? "send success" : "send failed");
    bsp_debug_log(line);
}

static runtime_pump_state_t map_pump_state(uint8_t state)
{
    return state == (uint8_t)PUMP_VFD_RUNNING ? RUNTIME_PUMP_RUNNING : RUNTIME_PUMP_STOPPED;
}

static power_domain_status_t scheduler_eval_pump_domain(const runtime_state_t *rs,
                                                        const control_config_t *control,
                                                        uint8_t pump_state,
                                                        const meter_snapshot_t *meter_snapshot)
{
    pump_control_mode_t mode = PUMP_CONTROL_RELAY_DIRECT;

    if (control != NULL) {
        mode = control->pump_control_mode;
    }
    if (pump_state != (uint8_t)PUMP_VFD_RUNNING) {
        return POWER_DOMAIN_LOST;
    }

    switch (mode) {
    case PUMP_CONTROL_METER_BREAKER_485:
        if (meter_snapshot == NULL || meter_snapshot->valid == 0U) {
            return POWER_DOMAIN_UNKNOWN;
        }
        if (rs != NULL && rs->runtime_sec == 0U && meter_snapshot->power_kw <= 0.05f) {
            return POWER_DOMAIN_UNKNOWN;
        }
        return meter_snapshot->power_kw > 0.05f ? POWER_DOMAIN_ON : POWER_DOMAIN_LOST;
    case PUMP_CONTROL_CONTACTOR_DIRECT:
    case PUMP_CONTROL_RELAY_DIRECT:
    default:
        return POWER_DOMAIN_ON;
    }
}

static void scheduler_refresh_snapshot(void)
{
    module_meter_values_t meter_values;
    module_flow_values_t flow_values;
    meter_snapshot_t meter_snapshot;
    flow_snapshot_t flow_snapshot;
    power_domain_status_t pump_domain = POWER_DOMAIN_UNKNOWN;
    power_domain_status_t valve_domain = POWER_DOMAIN_UNKNOWN;
    power_domain_status_t meter_domain = POWER_DOMAIN_UNKNOWN;
    uint8_t high_risk = 0U;
    float pressure_mpa = 0.0f;
    uint8_t pump_state = 0U;
    const runtime_state_t *rs = runtime_state_get();
    const control_config_t *control = config_store_control();

    memset(&meter_values, 0, sizeof(meter_values));
    memset(&flow_values, 0, sizeof(flow_values));
    memset(&meter_snapshot, 0, sizeof(meter_snapshot));
    memset(&flow_snapshot, 0, sizeof(flow_snapshot));
    (void)module_meter_query_values(&meter_values);
    (void)module_flow_query_values(&flow_values);
    (void)module_pump_vfd_query_state_u8(&pump_state);

    meter_snapshot.voltage_v = (float)meter_values.voltage_v;
    meter_snapshot.current_a = (float)meter_values.current_a;
    meter_snapshot.power_kw = (float)meter_values.power_kw;
    meter_snapshot.energy_kwh = (float)meter_values.energy_kwh;
    meter_snapshot.phase_status = meter_snapshot.voltage_v > 0.0f ? PHASE_STATUS_OK : PHASE_STATUS_LOSS;
    meter_snapshot.power_factor = 0.0f;
    meter_snapshot.frequency_hz = 50.0f;
    meter_snapshot.valid = meter_values.quality != 0U ? 1U : 0U;

    flow_snapshot.flow_m3h = flow_values.instant_m3h;
    flow_snapshot.total_m3 = (float)flow_values.total_m3;
    flow_snapshot.valid = flow_values.quality != 0U ? 1U : 0U;

    runtime_state_set_snapshot((float)meter_values.voltage_v,
                               (float)meter_values.current_a,
                               (float)meter_values.power_kw,
                               (float)meter_values.energy_kwh,
                               pressure_mpa,
                               flow_values.instant_m3h,
                               (float)flow_values.total_m3,
                               map_pump_state(pump_state),
                               RUNTIME_VALVE_CLOSED);
    runtime_state_set_meter_snapshots(NULL, &meter_snapshot, NULL);
    runtime_state_set_flow_snapshots(NULL, &flow_snapshot, NULL);

    meter_domain = meter_snapshot.valid != 0U ? POWER_DOMAIN_ON : POWER_DOMAIN_UNKNOWN;
    if (rs->pump_state == RUNTIME_PUMP_RUNNING) {
        pump_domain = scheduler_eval_pump_domain(rs, control, pump_state, &meter_snapshot);
    } else {
        pump_domain = POWER_DOMAIN_OFF;
    }
    valve_domain = rs->valve_state == RUNTIME_VALVE_OPEN || rs->valve_state == RUNTIME_VALVE_CLOSED
        ? POWER_DOMAIN_ON
        : POWER_DOMAIN_UNKNOWN;
    if (control != NULL && control->valve_fail_safe_mode == VALVE_HOLD_LAST) {
        high_risk = 1U;
    }
    runtime_state_set_power_domain_state(POWER_DOMAIN_ON, pump_domain, valve_domain, meter_domain, high_risk);
}

static int scheduler_send_heartbeat(void)
{
    bsp_debug_log("[PROTO] HEARTBEAT build begin\r\n");
    int n = telemetry_build_heartbeat(s_json_buffer, sizeof(s_json_buffer));
    if (n <= 0) {
        char line[96];
        (void)snprintf(line, sizeof(line), "[PROTO] HEARTBEAT build failed rc=%d\r\n", n);
        bsp_debug_log(line);
        return n;
    }
    {
        char line[96];
        (void)snprintf(line, sizeof(line), "[PROTO] HEARTBEAT build ok len=%d\r\n", n);
        bsp_debug_log(line);
    }
    scheduler_log_full_json("HEARTBEAT", s_json_buffer, (size_t)n);
    {
        int rc = net_connectivity_send_json(s_json_buffer, (size_t)n);
        if (rc >= 0) {
            runtime_state_inc_counter_heartbeat_ok();
        }
        scheduler_log_send("HEARTBEAT", rc);
        return rc;
    }
}

static int scheduler_send_snapshot(void)
{
    bsp_debug_log("[PROTO] STATE_SNAPSHOT build begin\r\n");
    int n = telemetry_build_state_snapshot(s_json_buffer, sizeof(s_json_buffer));
    if (n <= 0) {
        char line[104];
        (void)snprintf(line, sizeof(line), "[PROTO] STATE_SNAPSHOT build failed rc=%d\r\n", n);
        bsp_debug_log(line);
        return n;
    }
    {
        char line[104];
        (void)snprintf(line, sizeof(line), "[PROTO] STATE_SNAPSHOT build ok len=%d\r\n", n);
        bsp_debug_log(line);
    }
    scheduler_log_full_json("STATE_SNAPSHOT", s_json_buffer, (size_t)n);
    {
        int rc = net_connectivity_send_json(s_json_buffer, (size_t)n);
        if (rc >= 0) {
            runtime_state_inc_counter_snapshot_ok();
        }
        scheduler_log_send("STATE_SNAPSHOT", rc);
        return rc;
    }
}

static uint32_t scheduler_heartbeat_interval_ms(const device_config_t *cfg)
{
    uint32_t sec = 30U;

    if (cfg != NULL && cfg->runtime_rules.heartbeat_interval_sec > 0U) {
        sec = cfg->runtime_rules.heartbeat_interval_sec;
    }
    return sec * 1000U;
}

static uint32_t scheduler_snapshot_interval_ms(const device_config_t *cfg, const runtime_state_t *rs)
{
    uint32_t base_ms = 60000U;

    if (cfg != NULL && cfg->runtime_rules.snapshot_interval_sec > 0U) {
        base_ms = (uint32_t)cfg->runtime_rules.snapshot_interval_sec * 1000U;
    }
    if (rs == NULL) {
        return base_ms;
    }
    if (rs->protection_active || rs->workflow_state == RUNTIME_WORKFLOW_FAULT_LATCHED) {
        return 30000U;
    }
    if (rs->run_state == RUNTIME_RUN_RUNNING || rs->run_state == RUNTIME_RUN_STARTING || rs->run_state == RUNTIME_RUN_STOPPING) {
        return 30000U;
    }
    return base_ms;
}

void app_scheduler_init(void)
{
    s_last_100ms = 0U;
    s_last_1s = 0U;
    s_last_30s = 0U;
    s_last_heartbeat_ms = 0U;
    s_last_snapshot_ms = 0U;
    s_registered_window_open_ms = 0U;
    s_registered_window_open = 0U;
    s_prime_heartbeat_pending = 0U;
    s_prime_snapshot_pending = 0U;
    memset(s_json_buffer, 0, sizeof(s_json_buffer));
}

void app_scheduler_tick(uint32_t monotonic_ms)
{
    const device_config_t *cfg;
    const common_status_t *status;

    app_context()->monotonic_ms = monotonic_ms;
    workflow_card_reader_tick(monotonic_ms);
    workflow_voice_tick(monotonic_ms);
    safety_flow_tick(monotonic_ms);

    if ((uint32_t)(monotonic_ms - s_last_100ms) >= 100U) {
        s_last_100ms = monotonic_ms;
        module_registry_tick_100ms_all();
    }
    if ((uint32_t)(monotonic_ms - s_last_1s) >= 1000U) {
        s_last_1s = monotonic_ms;
        module_registry_tick_1s_all();
        scheduler_refresh_snapshot();
        safety_flow_poll_ready();
        safety_flow_poll_protection(monotonic_ms);
    }
    if ((uint32_t)(monotonic_ms - s_last_30s) >= 30000U) {
        s_last_30s = monotonic_ms;
        common_status_refresh_slow();
    }

    cfg = config_store_active();
    status = common_status_get();
    if (cfg == NULL || status == NULL || status->registered_once == false) {
        s_registered_window_open_ms = 0U;
        s_registered_window_open = 0U;
        s_prime_heartbeat_pending = 0U;
        s_prime_snapshot_pending = 0U;
        return;
    }

    if (s_registered_window_open == 0U) {
        s_registered_window_open = 1U;
        s_registered_window_open_ms = monotonic_ms;
        s_last_heartbeat_ms = monotonic_ms;
        s_last_snapshot_ms = monotonic_ms;
        s_prime_heartbeat_pending = 1U;
        s_prime_snapshot_pending = 1U;
        bsp_debug_log("[PROTO] registered window opened, defer HEARTBEAT 1500ms + STATE_SNAPSHOT 3000ms\r\n");
    }

    if (s_prime_heartbeat_pending != 0U &&
        (uint32_t)(monotonic_ms - s_registered_window_open_ms) >= SCHEDULER_REGISTER_PRIME_HEARTBEAT_DELAY_MS) {
        s_prime_heartbeat_pending = 0U;
        s_last_heartbeat_ms = monotonic_ms;
        if (scheduler_send_heartbeat() < 0) {
            return;
        }
    }
    if (s_prime_snapshot_pending != 0U &&
        (uint32_t)(monotonic_ms - s_registered_window_open_ms) >= SCHEDULER_REGISTER_PRIME_SNAPSHOT_DELAY_MS) {
        s_prime_snapshot_pending = 0U;
        s_last_snapshot_ms = monotonic_ms;
        if (scheduler_send_snapshot() < 0) {
            return;
        }
    }

    {
        uint32_t interval_ms = scheduler_heartbeat_interval_ms(cfg);
        if ((uint32_t)(monotonic_ms - s_last_heartbeat_ms) >= interval_ms) {
            s_last_heartbeat_ms = monotonic_ms;
            (void)scheduler_send_heartbeat();
        }
    }
    {
        uint32_t interval_ms = scheduler_snapshot_interval_ms(cfg, runtime_state_get());
        if ((uint32_t)(monotonic_ms - s_last_snapshot_ms) >= interval_ms) {
            s_last_snapshot_ms = monotonic_ms;
            (void)scheduler_send_snapshot();
        }
    }
}
