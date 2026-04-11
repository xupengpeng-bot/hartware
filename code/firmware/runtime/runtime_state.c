#include "runtime_state.h"

#include <string.h>

static runtime_state_t s_runtime_state;

static void copy_if_present(void *dst, const void *src, size_t n)
{
    if (dst != NULL && src != NULL) {
        memcpy(dst, src, n);
    }
}

void runtime_state_init(void)
{
    memset(&s_runtime_state, 0, sizeof(s_runtime_state));
    s_runtime_state.workflow_state = RUNTIME_WORKFLOW_BOOTING;
    s_runtime_state.run_state = RUNTIME_RUN_STANDBY;
    s_runtime_state.power_state = RUNTIME_POWER_MAINS;
    s_runtime_state.card_state = CARD_STATE_IDLE;
    s_runtime_state.protection_state = SAFE_STATE_NORMAL;
    s_runtime_state.blocked_reason = BLOCKED_NONE;
    s_runtime_state.last_voice_prompt = VOICE_DEVICE_BOOTING;
    s_runtime_state.pump_state = RUNTIME_PUMP_STOPPED;
    s_runtime_state.valve_state = RUNTIME_VALVE_CLOSED;
    s_runtime_state.protection_reason = RUNTIME_PROTECT_NONE;
    s_runtime_state.target_type = TARGET_PUMP;
    s_runtime_state.linkage_mode = LINKAGE_LOCAL_INTEGRATED;
    s_runtime_state.settlement_mode = SETTLEMENT_BY_ENERGY_KWH;
    s_runtime_state.power_domains.controller_power_domain = POWER_DOMAIN_ON;
    s_runtime_state.power_domains.pump_power_domain = POWER_DOMAIN_UNKNOWN;
    s_runtime_state.power_domains.valve_power_domain = POWER_DOMAIN_UNKNOWN;
    s_runtime_state.power_domains.meter_power_domain = POWER_DOMAIN_UNKNOWN;
}

runtime_state_t *runtime_state_mutable(void)
{
    return &s_runtime_state;
}

const runtime_state_t *runtime_state_get(void)
{
    return &s_runtime_state;
}

void runtime_state_set_online(bool v) { s_runtime_state.online = v; }
void runtime_state_set_tcp_connected(bool v) { s_runtime_state.tcp_connected = v; }
void runtime_state_set_registered(bool v) { s_runtime_state.registered = v; }
void runtime_state_set_ready(bool v) { s_runtime_state.ready = v; }
void runtime_state_set_time_synced(bool v) { s_runtime_state.time_synced = v; }
void runtime_state_set_config_version(uint32_t version) { s_runtime_state.config_version = version; }
void runtime_state_set_signal_csq(int16_t csq) { s_runtime_state.signal_csq = csq; }
void runtime_state_set_battery_soc(uint8_t soc) { s_runtime_state.battery_soc = soc; }
void runtime_state_set_power_state(runtime_power_state_t state) { s_runtime_state.power_state = state; }
void runtime_state_set_workflow_state(runtime_workflow_state_t state) { s_runtime_state.workflow_state = state; }
void runtime_state_set_run_state(runtime_run_state_t state) { s_runtime_state.run_state = state; }
void runtime_state_set_card_state(card_flow_state_t state) { s_runtime_state.card_state = state; }
void runtime_state_set_safe_state(safety_state_t state) { s_runtime_state.protection_state = state; }
void runtime_state_set_blocked_reason(blocked_reason_code_t reason) { s_runtime_state.blocked_reason = reason; }
void runtime_state_set_last_event(runtime_event_t event_id) { s_runtime_state.last_event = event_id; }
void runtime_state_set_voice_prompt(voice_prompt_code_t code) { s_runtime_state.last_voice_prompt = code; }
void runtime_state_set_runtime_sec(uint32_t runtime_sec) { s_runtime_state.runtime_sec = runtime_sec; }
void runtime_state_set_stop_reason(session_stop_reason_t reason) { s_runtime_state.stop_reason = reason; }
void runtime_state_set_pump_state(runtime_pump_state_t state) { s_runtime_state.pump_state = state; }
void runtime_state_set_valve_state(runtime_valve_state_t state) { s_runtime_state.valve_state = state; }
void runtime_state_set_meter_epoch(uint32_t meter_epoch) { s_runtime_state.meter_epoch = meter_epoch; }

void runtime_state_set_alarm_codes(const uint32_t *codes, size_t count)
{
    size_t n = count > RUNTIME_ALARM_MAX ? RUNTIME_ALARM_MAX : count;
    memset(s_runtime_state.alarm_codes, 0, sizeof(s_runtime_state.alarm_codes));
    s_runtime_state.alarm_count = 0U;
    if (codes == NULL || n == 0U) {
        return;
    }
    memcpy(s_runtime_state.alarm_codes, codes, n * sizeof(uint32_t));
    s_runtime_state.alarm_count = (uint8_t)n;
}

void runtime_state_set_snapshot(float voltage_v, float current_a, float power_kw, float energy_kwh,
                                float pressure_mpa, float flow_m3h, float total_m3,
                                runtime_pump_state_t pump_state, runtime_valve_state_t valve_state)
{
    s_runtime_state.voltage_v = voltage_v;
    s_runtime_state.current_a = current_a;
    s_runtime_state.power_kw = power_kw;
    s_runtime_state.energy_kwh = energy_kwh;
    s_runtime_state.pressure_mpa = pressure_mpa;
    s_runtime_state.flow_m3h = flow_m3h;
    s_runtime_state.total_m3 = total_m3;
    s_runtime_state.pump_state = pump_state;
    s_runtime_state.valve_state = valve_state;
}

void runtime_state_set_meter_snapshots(const meter_snapshot_t *start, const meter_snapshot_t *last, const meter_snapshot_t *stop)
{
    copy_if_present(&s_runtime_state.meter_start, start, sizeof(s_runtime_state.meter_start));
    copy_if_present(&s_runtime_state.meter_last, last, sizeof(s_runtime_state.meter_last));
    copy_if_present(&s_runtime_state.meter_stop, stop, sizeof(s_runtime_state.meter_stop));
}

void runtime_state_set_flow_snapshots(const flow_snapshot_t *start, const flow_snapshot_t *last, const flow_snapshot_t *stop)
{
    copy_if_present(&s_runtime_state.flow_start, start, sizeof(s_runtime_state.flow_start));
    copy_if_present(&s_runtime_state.flow_last, last, sizeof(s_runtime_state.flow_last));
    copy_if_present(&s_runtime_state.flow_stop, stop, sizeof(s_runtime_state.flow_stop));
}

void runtime_state_set_session_ref(const char *session_ref)
{
    memset(s_runtime_state.session_ref, 0, sizeof(s_runtime_state.session_ref));
    if (session_ref != NULL) {
        (void)strncpy(s_runtime_state.session_ref, session_ref, sizeof(s_runtime_state.session_ref) - 1U);
    }
    memset(s_runtime_state.summary.session_ref, 0, sizeof(s_runtime_state.summary.session_ref));
    if (session_ref != NULL) {
        (void)strncpy(s_runtime_state.summary.session_ref, session_ref, sizeof(s_runtime_state.summary.session_ref) - 1U);
    }
}

void runtime_state_set_session_modes(session_target_type_t target_type, linkage_mode_t linkage_mode,
                                     settlement_mode_t settlement_mode)
{
    s_runtime_state.target_type = target_type;
    s_runtime_state.linkage_mode = linkage_mode;
    s_runtime_state.settlement_mode = settlement_mode;
    s_runtime_state.summary.target_type = target_type;
    s_runtime_state.summary.linkage_mode = linkage_mode;
    s_runtime_state.summary.settlement_mode = settlement_mode;
}

void runtime_state_set_session_lease(const session_lease_t *lease)
{
    if (lease != NULL) {
        s_runtime_state.lease = *lease;
    } else {
        memset(&s_runtime_state.lease, 0, sizeof(s_runtime_state.lease));
    }
}

void runtime_state_set_power_domain_state(power_domain_status_t controller_state,
                                          power_domain_status_t pump_state,
                                          power_domain_status_t valve_state,
                                          power_domain_status_t meter_state,
                                          uint8_t high_risk)
{
    s_runtime_state.power_domains.controller_power_domain = controller_state;
    s_runtime_state.power_domains.pump_power_domain = pump_state;
    s_runtime_state.power_domains.valve_power_domain = valve_state;
    s_runtime_state.power_domains.meter_power_domain = meter_state;
    s_runtime_state.power_domains.power_loss_high_risk = high_risk;
}

void runtime_state_set_protection(runtime_protect_reason_t reason, bool active)
{
    s_runtime_state.protection_active = active;
    s_runtime_state.protection_reason = active ? reason : RUNTIME_PROTECT_NONE;
    if (active) {
        s_runtime_state.protection_state = SAFE_STATE_LATCHED;
        s_runtime_state.run_state = RUNTIME_RUN_FAULT_LATCHED;
        s_runtime_state.workflow_state = RUNTIME_WORKFLOW_FAULT_LATCHED;
    } else {
        s_runtime_state.protection_state = SAFE_STATE_NORMAL;
    }
}

void runtime_state_inc_counter_register_ok(void) { s_runtime_state.counters.register_ok_count++; }
void runtime_state_inc_counter_heartbeat_ok(void) { s_runtime_state.counters.heartbeat_ok_count++; }
void runtime_state_inc_counter_snapshot_ok(void) { s_runtime_state.counters.snapshot_ok_count++; }
void runtime_state_inc_counter_event_report(void) { s_runtime_state.counters.event_report_count++; }
void runtime_state_inc_counter_protection_trip(void) { s_runtime_state.counters.protection_trip_count++; }
void runtime_state_inc_counter_session_start(void) { s_runtime_state.counters.session_start_count++; }
void runtime_state_inc_counter_session_stop(void) { s_runtime_state.counters.session_stop_count++; }
void runtime_state_inc_counter_network_lost(void) { s_runtime_state.counters.network_lost_count++; }
void runtime_state_inc_counter_power_loss(void) { s_runtime_state.counters.power_loss_count++; }

const char *runtime_state_workflow_name(runtime_workflow_state_t state)
{
    switch (state) {
    case RUNTIME_WORKFLOW_BOOTING: return "BOOTING";
    case RUNTIME_WORKFLOW_NOT_READY: return "NOT_READY";
    case RUNTIME_WORKFLOW_READY_IDLE: return "READY_IDLE";
    case RUNTIME_WORKFLOW_AUTH_PENDING: return "AUTH_PENDING";
    case RUNTIME_WORKFLOW_PRE_START_METERING: return "PRE_START_METERING";
    case RUNTIME_WORKFLOW_START_SEQUENCE: return "START_SEQUENCE";
    case RUNTIME_WORKFLOW_RUNNING: return "RUNNING";
    case RUNTIME_WORKFLOW_STOP_SEQUENCE: return "STOP_SEQUENCE";
    case RUNTIME_WORKFLOW_POST_STOP_METERING: return "POST_STOP_METERING";
    case RUNTIME_WORKFLOW_BLOCKED: return "BLOCKED";
    case RUNTIME_WORKFLOW_FAULT_LATCHED: return "FAULT_LATCHED";
    case RUNTIME_WORKFLOW_RECOVERY_LOCKED: return "RECOVERY_LOCKED";
    default: return "NOT_READY";
    }
}

const char *runtime_state_run_name(runtime_run_state_t state)
{
    switch (state) {
    case RUNTIME_RUN_STANDBY: return "standby";
    case RUNTIME_RUN_STARTING: return "starting";
    case RUNTIME_RUN_RUNNING: return "running";
    case RUNTIME_RUN_STOPPING: return "stopping";
    case RUNTIME_RUN_FAULT_LATCHED: return "fault_latched";
    case RUNTIME_RUN_RECOVERY_LOCKED: return "recovery_locked";
    default: return "standby";
    }
}

const char *runtime_state_power_name(runtime_power_state_t state)
{
    switch (state) {
    case RUNTIME_POWER_MAINS: return "mains";
    case RUNTIME_POWER_BATTERY: return "battery";
    case RUNTIME_POWER_SOLAR: return "solar";
    default: return "mains";
    }
}

const char *runtime_state_pump_name(runtime_pump_state_t state)
{
    return state == RUNTIME_PUMP_RUNNING ? "running" : "stopped";
}

const char *runtime_state_valve_name(runtime_valve_state_t state)
{
    return state == RUNTIME_VALVE_OPEN ? "open" : "closed";
}

const char *runtime_state_card_name(card_flow_state_t state)
{
    switch (state) {
    case CARD_STATE_IDLE: return "CARD_IDLE";
    case CARD_STATE_READ: return "CARD_READ";
    case CARD_STATE_DEBOUNCE: return "CARD_DEBOUNCE";
    case CARD_STATE_READY_CHECK: return "CARD_READY_CHECK";
    case CARD_STATE_VOICE_PROMPT: return "CARD_VOICE_PROMPT";
    case CARD_STATE_CLOUD_AUTH_PENDING: return "CARD_CLOUD_AUTH_PENDING";
    case CARD_STATE_AUTH_GRANTED: return "CARD_AUTH_GRANTED";
    case CARD_STATE_AUTH_DENIED: return "CARD_AUTH_DENIED";
    case CARD_STATE_STARTING: return "CARD_STARTING";
    case CARD_STATE_COMPLETED: return "CARD_COMPLETED";
    case CARD_STATE_BLOCKED: return "CARD_BLOCKED";
    default: return "CARD_IDLE";
    }
}

const char *runtime_state_safe_name(safety_state_t state)
{
    switch (state) {
    case SAFE_STATE_NORMAL: return "SAFE_NORMAL";
    case SAFE_STATE_REJECT_START: return "SAFE_REJECT_START";
    case SAFE_STATE_STOPPING: return "SAFE_STOPPING";
    case SAFE_STATE_LATCHED: return "SAFE_LATCHED";
    default: return "SAFE_NORMAL";
    }
}

const char *runtime_state_event_name(runtime_event_t event_id)
{
    switch (event_id) {
    case EVT_BOOT_OK: return "EVT_BOOT_OK";
    case EVT_BOOT_FAIL: return "EVT_BOOT_FAIL";
    case EVT_SCAN_START_REQUEST: return "EVT_SCAN_START_REQUEST";
    case EVT_CARD_SWIPED: return "EVT_CARD_SWIPED";
    case EVT_READY_CHECK_OK: return "EVT_READY_CHECK_OK";
    case EVT_READY_CHECK_FAIL: return "EVT_READY_CHECK_FAIL";
    case EVT_CLOUD_AUTH_GRANTED: return "EVT_CLOUD_AUTH_GRANTED";
    case EVT_CLOUD_AUTH_DENIED: return "EVT_CLOUD_AUTH_DENIED";
    case EVT_PRE_START_METERING_OK: return "EVT_PRE_START_METERING_OK";
    case EVT_PRE_START_METERING_FAIL: return "EVT_PRE_START_METERING_FAIL";
    case EVT_START_SEQUENCE_OK: return "EVT_START_SEQUENCE_OK";
    case EVT_START_SEQUENCE_FAIL: return "EVT_START_SEQUENCE_FAIL";
    case EVT_RUNNING_TICK: return "EVT_RUNNING_TICK";
    case EVT_STOP_REQUEST: return "EVT_STOP_REQUEST";
    case EVT_STOP_SEQUENCE_OK: return "EVT_STOP_SEQUENCE_OK";
    case EVT_POST_STOP_METERING_OK: return "EVT_POST_STOP_METERING_OK";
    case EVT_POST_STOP_METERING_FAIL: return "EVT_POST_STOP_METERING_FAIL";
    case EVT_PROTECTION_TRIGGERED: return "EVT_PROTECTION_TRIGGERED";
    case EVT_NETWORK_LOST: return "EVT_NETWORK_LOST";
    case EVT_NETWORK_RECOVERED: return "EVT_NETWORK_RECOVERED";
    case EVT_PUMP_POWER_LOST: return "EVT_PUMP_POWER_LOST";
    case EVT_CONTROLLER_POWER_RESTORED: return "EVT_CONTROLLER_POWER_RESTORED";
    case EVT_FULL_POWER_RESTORE_BOOT: return "EVT_FULL_POWER_RESTORE_BOOT";
    case EVT_SESSION_LEASE_EXPIRED: return "EVT_SESSION_LEASE_EXPIRED";
    case EVT_CONFIG_UPDATED: return "EVT_CONFIG_UPDATED";
    default: return "EVT_BOOT_OK";
    }
}

const char *runtime_state_blocked_reason_name(blocked_reason_code_t code)
{
    switch (code) {
    case BLOCKED_NOT_READY_BOOTING: return "NOT_READY_BOOTING";
    case BLOCKED_NOT_READY_CONFIG: return "NOT_READY_CONFIG";
    case BLOCKED_PROTECTION_LATCHED: return "PROTECTION_LATCHED";
    case BLOCKED_ALREADY_RUNNING: return "ALREADY_RUNNING";
    case BLOCKED_RESOURCE_BUSY: return "RESOURCE_BUSY";
    case BLOCKED_POWER_ABNORMAL: return "POWER_ABNORMAL";
    case BLOCKED_OVER_VOLTAGE: return "OVER_VOLTAGE";
    case BLOCKED_UNDER_VOLTAGE: return "UNDER_VOLTAGE";
    case BLOCKED_PHASE_LOSS: return "PHASE_LOSS";
    case BLOCKED_OVER_CURRENT_LOCKED: return "OVER_CURRENT_LOCKED";
    case BLOCKED_PRESSURE_ABNORMAL: return "PRESSURE_ABNORMAL";
    case BLOCKED_DRY_RUN_RISK: return "DRY_RUN_RISK";
    case BLOCKED_NETWORK_UNAVAILABLE: return "NETWORK_UNAVAILABLE";
    case BLOCKED_METER_UNAVAILABLE: return "METER_UNAVAILABLE";
    case BLOCKED_FLOW_SENSOR_UNAVAILABLE: return "FLOW_SENSOR_UNAVAILABLE";
    case BLOCKED_RECOVERY_LOCKED: return "RECOVERY_LOCKED";
    case BLOCKED_SESSION_LEASE_INVALID: return "SESSION_LEASE_INVALID";
    case BLOCKED_NONE:
    default: return "NONE";
    }
}

const char *runtime_state_voice_prompt_name(voice_prompt_code_t code)
{
    switch (code) {
    case VOICE_DEVICE_BOOTING: return "DEVICE_BOOTING";
    case VOICE_DEVICE_NOT_READY: return "DEVICE_NOT_READY";
    case VOICE_DEVICE_CONFIG_MISSING: return "DEVICE_CONFIG_MISSING";
    case VOICE_NETWORK_UNAVAILABLE: return "NETWORK_UNAVAILABLE";
    case VOICE_AUTH_CHECKING: return "AUTH_CHECKING";
    case VOICE_AUTH_GRANTED: return "AUTH_GRANTED";
    case VOICE_AUTH_DENIED: return "AUTH_DENIED";
    case VOICE_METER_QUERY_FAILED: return "METER_QUERY_FAILED";
    case VOICE_STARTING_PUMP: return "STARTING_PUMP";
    case VOICE_STARTING_VALVE: return "STARTING_VALVE";
    case VOICE_START_SUCCESS: return "START_SUCCESS";
    case VOICE_START_FAILED: return "START_FAILED";
    case VOICE_ALREADY_RUNNING: return "ALREADY_RUNNING";
    case VOICE_STOPPING: return "STOPPING";
    case VOICE_STOPPED: return "STOPPED";
    case VOICE_PROTECTION_TRIGGERED: return "PROTECTION_TRIGGERED";
    case VOICE_POWER_INTERRUPTED: return "POWER_INTERRUPTED";
    case VOICE_POWER_RESTORED_SELF_CHECK: return "POWER_RESTORED_SELF_CHECK";
    case VOICE_RECOVERY_LOCKED_CONFIRM_REQUIRED: return "RECOVERY_LOCKED_CONFIRM_REQUIRED";
    default: return "DEVICE_BOOTING";
    }
}

const char *runtime_state_target_name(session_target_type_t target)
{
    switch (target) {
    case TARGET_PUMP: return "pump";
    case TARGET_VALVE: return "valve";
    case TARGET_PUMP_WITH_VALVE: return "pump_with_valve";
    default: return "pump";
    }
}

const char *runtime_state_linkage_name(linkage_mode_t mode)
{
    switch (mode) {
    case LINKAGE_LOCAL_INTEGRATED: return "local_integrated";
    case LINKAGE_PLATFORM_ORCHESTRATED: return "platform_orchestrated";
    default: return "local_integrated";
    }
}

const char *runtime_state_settlement_name(settlement_mode_t mode)
{
    switch (mode) {
    case SETTLEMENT_BY_DURATION_HOUR: return "by_duration_hour";
    case SETTLEMENT_BY_ENERGY_KWH: return "by_energy_kwh";
    case SETTLEMENT_BY_WATER_M3: return "by_water_m3";
    default: return "by_energy_kwh";
    }
}

const char *runtime_state_stop_reason_name(session_stop_reason_t reason)
{
    switch (reason) {
    case STOP_REASON_PLATFORM_STOP: return "platform_stop";
    case STOP_REASON_LOCAL_STOP: return "local_stop";
    case STOP_REASON_PROTECTION_TRIP: return "protection_trip";
    case STOP_REASON_SESSION_LEASE_EXPIRED: return "session_lease_expired";
    case STOP_REASON_PUMP_POWER_LOSS: return "pump_power_loss";
    case STOP_REASON_CONTROLLER_POWER_INTERRUPT: return "controller_power_interrupt";
    case STOP_REASON_NONE:
    default: return "none";
    }
}

const char *runtime_state_power_domain_name(power_domain_status_t state)
{
    switch (state) {
    case POWER_DOMAIN_OFF: return "off";
    case POWER_DOMAIN_ON: return "on";
    case POWER_DOMAIN_LOST: return "lost";
    case POWER_DOMAIN_UNKNOWN:
    default: return "unknown";
    }
}
