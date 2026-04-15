#ifndef MODEL_RUNTIME_H
#define MODEL_RUNTIME_H

#include "model_config.h"
#include "model_types.h"

#include <stdbool.h>
#include <stdint.h>

#define MODEL_SESSION_ID_LEN 64U
#define MODEL_RECOVERY_HINT_LEN 64U

typedef struct {
    char     session_id[MODEL_SESSION_ID_LEN];
    uint32_t started_at_utc;
    uint8_t  reserved[8];
} workflow_session_context_t;

typedef struct {
    bool     recovery_pending;
    bool     settlement_pending;
    uint32_t last_stop_reason_code;
    uint32_t last_stop_at_utc;
    uint32_t last_session_started_at_utc;
    char     last_session_id[MODEL_SESSION_ID_LEN];
    char     last_recovery_hint[MODEL_RECOVERY_HINT_LEN];
} workflow_recovery_context_t;

typedef struct {
    char             session_ref[CTRL_SESSION_REF_LEN];
    uint16_t         session_lease_sec;
    uint32_t         lease_expire_at;
    uint8_t          keepalive_required;
    uint32_t         last_lease_refresh_at;
} session_lease_t;

typedef struct {
    float          voltage_v;
    float          current_a;
    float          power_kw;
    float          energy_kwh;
    phase_status_t phase_status;
    float          power_factor;
    float          frequency_hz;
    uint8_t        valid;
} meter_snapshot_t;

typedef struct {
    float   flow_m3h;
    float   total_m3;
    uint8_t valid;
} flow_snapshot_t;

typedef struct {
    power_domain_status_t controller_power_domain;
    power_domain_status_t pump_power_domain;
    power_domain_status_t valve_power_domain;
    power_domain_status_t meter_power_domain;
    uint8_t               power_loss_high_risk;
} power_domain_model_t;

typedef struct {
    char                  session_ref[CTRL_SESSION_REF_LEN];
    char                  meter_source[24];
    session_target_type_t target_type;
    linkage_mode_t        linkage_mode;
    settlement_mode_t     settlement_mode;
    uint32_t              runtime_sec;
    float                 start_energy_kwh;
    float                 stop_energy_kwh;
    float                 delta_energy_kwh;
    float                 start_total_m3;
    float                 stop_total_m3;
    float                 delta_total_m3;
    float                 last_voltage_v;
    float                 last_current_a;
    float                 last_power_kw;
    session_stop_reason_t stop_reason;
    uint8_t               finalized;
} session_summary_t;

typedef struct {
    uint32_t               magic;
    uint8_t                last_session_active;
    session_main_state_t   last_main_state;
    session_target_type_t  last_target_type;
    linkage_mode_t         last_linkage_mode;
    valve_fail_safe_mode_t valve_fail_safe_mode;
    pump_output_fail_safe_t pump_output_fail_safe;
    session_stop_reason_t  last_stop_reason;
    uint8_t                power_loss_high_risk;
    uint8_t                finalized;
    char                   session_ref[CTRL_SESSION_REF_LEN];
    uint32_t               runtime_sec;
    float                  last_energy_kwh;
    float                  last_total_m3;
} power_loss_min_persist_t;

typedef struct {
    uint32_t register_ok_count;
    uint32_t heartbeat_ok_count;
    uint32_t snapshot_ok_count;
    uint32_t event_report_count;
    uint32_t protection_trip_count;
    uint32_t session_start_count;
    uint32_t session_stop_count;
    uint32_t network_lost_count;
    uint32_t power_loss_count;
} runtime_counters_t;

typedef struct {
    uint8_t  pending;
    uint32_t epoch;
    char     reason[8];
    char     session_ref[CTRL_SESSION_REF_LEN];
    uint32_t runtime_sec;
    float    total_m3;
    float    energy_kwh;
} counter_reset_persist_t;

typedef struct {
    workflow_state_t            workflow_state;
    workflow_session_context_t  active_session;
    workflow_recovery_context_t recovery;
    uint64_t                    flow_pulse_total;
    double                      energy_kwh_total;
    uint32_t                    meter_epoch;
    uint32_t                    last_reported_counter_reset_epoch;
    uint8_t                     meter_identity_valid;
    uint8_t                     meter_addr_bcd[6];
    uint8_t                     meter_protocol_variant;
    uint8_t                     reserved0;
    counter_reset_persist_t     counter_reset;
    power_loss_min_persist_t    power_loss_persist;
} device_runtime_t;

#endif /* MODEL_RUNTIME_H */
