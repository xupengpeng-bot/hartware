#ifndef RUNTIME_STATE_H
#define RUNTIME_STATE_H

#include "model_runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RUNTIME_ALARM_MAX 8U

typedef enum {
    RUNTIME_WORKFLOW_BOOTING = 0,
    RUNTIME_WORKFLOW_NOT_READY,
    RUNTIME_WORKFLOW_READY_IDLE,
    RUNTIME_WORKFLOW_AUTH_PENDING,
    RUNTIME_WORKFLOW_PRE_START_METERING,
    RUNTIME_WORKFLOW_START_SEQUENCE,
    RUNTIME_WORKFLOW_RUNNING,
    RUNTIME_WORKFLOW_STOP_SEQUENCE,
    RUNTIME_WORKFLOW_POST_STOP_METERING,
    RUNTIME_WORKFLOW_BLOCKED,
    RUNTIME_WORKFLOW_FAULT_LATCHED,
    RUNTIME_WORKFLOW_RECOVERY_LOCKED
} runtime_workflow_state_t;

typedef enum {
    RUNTIME_RUN_STANDBY = 0,
    RUNTIME_RUN_STARTING,
    RUNTIME_RUN_RUNNING,
    RUNTIME_RUN_STOPPING,
    RUNTIME_RUN_FAULT_LATCHED,
    RUNTIME_RUN_RECOVERY_LOCKED
} runtime_run_state_t;

typedef enum {
    RUNTIME_POWER_MAINS = 0,
    RUNTIME_POWER_BATTERY,
    RUNTIME_POWER_SOLAR
} runtime_power_state_t;

typedef enum {
    RUNTIME_PUMP_STOPPED = 0,
    RUNTIME_PUMP_RUNNING
} runtime_pump_state_t;

typedef enum {
    RUNTIME_VALVE_CLOSED = 0,
    RUNTIME_VALVE_OPEN
} runtime_valve_state_t;

typedef enum {
    RUNTIME_PROTECT_NONE = 0,
    RUNTIME_PROTECT_OVERLOAD,
    RUNTIME_PROTECT_PHASE_LOSS,
    RUNTIME_PROTECT_UNDER_VOLTAGE,
    RUNTIME_PROTECT_OVER_VOLTAGE,
    RUNTIME_PROTECT_DRY_RUN,
    RUNTIME_PROTECT_PRESSURE_HIGH,
    RUNTIME_PROTECT_PRESSURE_LOW,
    RUNTIME_PROTECT_INTERLOCK,
    RUNTIME_PROTECT_PUMP_POWER_LOSS
} runtime_protect_reason_t;

typedef struct {
    bool                  online;
    bool                  tcp_connected;
    bool                  registered;
    bool                  ready;
    bool                  time_synced;
    uint32_t              config_version;
    runtime_workflow_state_t workflow_state;
    runtime_run_state_t   run_state;
    runtime_power_state_t power_state;
    card_flow_state_t     card_state;
    safety_state_t        protection_state;
    blocked_reason_code_t blocked_reason;
    runtime_event_t       last_event;
    voice_prompt_code_t   last_voice_prompt;
    int16_t               signal_csq;
    uint8_t               battery_soc;
    uint32_t              alarm_codes[RUNTIME_ALARM_MAX];
    uint8_t               alarm_count;
    float                 voltage_v;
    float                 current_a;
    float                 power_kw;
    float                 energy_kwh;
    uint32_t              meter_epoch;
    float                 pressure_mpa;
    float                 flow_m3h;
    float                 total_m3;
    uint32_t              runtime_sec;
    runtime_pump_state_t  pump_state;
    runtime_valve_state_t valve_state;
    bool                  protection_active;
    runtime_protect_reason_t protection_reason;
    session_target_type_t target_type;
    linkage_mode_t        linkage_mode;
    settlement_mode_t     settlement_mode;
    session_stop_reason_t stop_reason;
    char                  session_ref[CTRL_SESSION_REF_LEN];
    session_lease_t       lease;
    meter_snapshot_t      meter_start;
    meter_snapshot_t      meter_last;
    meter_snapshot_t      meter_stop;
    flow_snapshot_t       flow_start;
    flow_snapshot_t       flow_last;
    flow_snapshot_t       flow_stop;
    power_domain_model_t  power_domains;
    session_summary_t     summary;
    runtime_counters_t    counters;
    uint8_t               recovery_event_pending;
    uint8_t               boot_ok;
} runtime_state_t;

void runtime_state_init(void);
runtime_state_t *runtime_state_mutable(void);
const runtime_state_t *runtime_state_get(void);

void runtime_state_set_online(bool v);
void runtime_state_set_tcp_connected(bool v);
void runtime_state_set_registered(bool v);
void runtime_state_set_ready(bool v);
void runtime_state_set_time_synced(bool v);
void runtime_state_set_config_version(uint32_t version);
void runtime_state_set_signal_csq(int16_t csq);
void runtime_state_set_battery_soc(uint8_t soc);
void runtime_state_set_power_state(runtime_power_state_t state);
void runtime_state_set_alarm_codes(const uint32_t *codes, size_t count);
void runtime_state_set_workflow_state(runtime_workflow_state_t state);
void runtime_state_set_run_state(runtime_run_state_t state);
void runtime_state_set_card_state(card_flow_state_t state);
void runtime_state_set_safe_state(safety_state_t state);
void runtime_state_set_blocked_reason(blocked_reason_code_t reason);
void runtime_state_set_last_event(runtime_event_t event_id);
void runtime_state_set_voice_prompt(voice_prompt_code_t code);
void runtime_state_set_snapshot(float voltage_v, float current_a, float power_kw, float energy_kwh,
                                float pressure_mpa, float flow_m3h, float total_m3,
                                runtime_pump_state_t pump_state, runtime_valve_state_t valve_state);
void runtime_state_set_meter_epoch(uint32_t meter_epoch);
void runtime_state_set_meter_snapshots(const meter_snapshot_t *start, const meter_snapshot_t *last, const meter_snapshot_t *stop);
void runtime_state_set_flow_snapshots(const flow_snapshot_t *start, const flow_snapshot_t *last, const flow_snapshot_t *stop);
void runtime_state_set_session_ref(const char *session_ref);
void runtime_state_set_session_modes(session_target_type_t target_type, linkage_mode_t linkage_mode,
                                     settlement_mode_t settlement_mode);
void runtime_state_set_runtime_sec(uint32_t runtime_sec);
void runtime_state_set_session_lease(const session_lease_t *lease);
void runtime_state_set_stop_reason(session_stop_reason_t reason);
void runtime_state_set_power_domain_state(power_domain_status_t controller_state,
                                          power_domain_status_t pump_state,
                                          power_domain_status_t valve_state,
                                          power_domain_status_t meter_state,
                                          uint8_t high_risk);
void runtime_state_set_protection(runtime_protect_reason_t reason, bool active);
void runtime_state_set_pump_state(runtime_pump_state_t state);
void runtime_state_set_valve_state(runtime_valve_state_t state);
void runtime_state_inc_counter_register_ok(void);
void runtime_state_inc_counter_heartbeat_ok(void);
void runtime_state_inc_counter_snapshot_ok(void);
void runtime_state_inc_counter_event_report(void);
void runtime_state_inc_counter_protection_trip(void);
void runtime_state_inc_counter_session_start(void);
void runtime_state_inc_counter_session_stop(void);
void runtime_state_inc_counter_network_lost(void);
void runtime_state_inc_counter_power_loss(void);

const char *runtime_state_workflow_name(runtime_workflow_state_t state);
const char *runtime_state_run_name(runtime_run_state_t state);
const char *runtime_state_power_name(runtime_power_state_t state);
const char *runtime_state_pump_name(runtime_pump_state_t state);
const char *runtime_state_valve_name(runtime_valve_state_t state);
const char *runtime_state_card_name(card_flow_state_t state);
const char *runtime_state_safe_name(safety_state_t state);
const char *runtime_state_event_name(runtime_event_t event_id);
const char *runtime_state_blocked_reason_name(blocked_reason_code_t code);
const char *runtime_state_voice_prompt_name(voice_prompt_code_t code);
const char *runtime_state_target_name(session_target_type_t target);
const char *runtime_state_linkage_name(linkage_mode_t mode);
const char *runtime_state_settlement_name(settlement_mode_t mode);
const char *runtime_state_stop_reason_name(session_stop_reason_t reason);
const char *runtime_state_power_domain_name(power_domain_status_t state);

#endif /* RUNTIME_STATE_H */
