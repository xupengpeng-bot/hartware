#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

#include "model_types.h"

#define MODEL_MAX_CHANNEL_BINDINGS 8U
#define MODEL_PLATFORM_HOST_MAX 96U
#define MODEL_DEFAULT_TIME_ZONE_QUARTER_HOURS 32

typedef struct {
    uint8_t  overload_protection;
    float    over_current_limit_a;
    uint8_t  phase_loss_protection;
    uint8_t  under_voltage_protection;
    float    under_voltage_limit_v;
    uint8_t  over_voltage_protection;
    float    over_voltage_limit_v;
    uint8_t  dry_run_protection;
    float    pressure_high_limit;
    float    pressure_low_limit;
    uint32_t start_delay_ms;
    uint32_t stop_delay_ms;
} protection_config_t;

typedef struct {
    pump_control_mode_t     pump_control_mode;
    valve_control_mode_t    valve_control_mode;
    pump_output_fail_safe_t pump_output_fail_safe;
    valve_fail_safe_mode_t  valve_fail_safe_mode;
    linkage_mode_t          linkage_mode;
    uint16_t                valve_open_pulse_ms;
    uint16_t                valve_close_pulse_ms;
    uint8_t                 offline_new_start_enabled;
    uint8_t                 power_restore_resume_enabled;
    uint8_t                 cross_device_linkage_from_edge;
    uint16_t                offline_max_runtime_sec;
} control_config_t;

typedef struct {
    uint32_t            config_version;
    feature_modules_t   feature_modules;
    uint16_t            channel_binding_count;
    channel_binding_t   channel_bindings[MODEL_MAX_CHANNEL_BINDINGS];
    runtime_rules_t     runtime_rules;
    protection_config_t protection_config;
    control_config_t    control_config;
    char                platform_tcp_host[MODEL_PLATFORM_HOST_MAX];
    uint16_t            platform_tcp_port;
    int16_t             time_zone_quarter_hours;
} device_config_t;

#endif /* MODEL_CONFIG_H */
