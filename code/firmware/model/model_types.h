/**
 * Core identity, inventory, bindings, runtime rules, workflow states — Firmware Dev Spec v1 §5–6.
 */
#ifndef MODEL_TYPES_H
#define MODEL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define CTRL_IMEI_LEN           24U
#define CTRL_ICCID_LEN          32U
#define CTRL_HW_SKU_LEN         32U
#define CTRL_HW_REV_LEN         16U
#define CTRL_FW_FAMILY_LEN      32U
#define CTRL_FW_VERSION_LEN     32U
#define CTRL_CHANNEL_CODE_LEN   32U
#define CTRL_MODULE_CODE_LEN    32U
#define CTRL_CHANNEL_ROLE_LEN   32U
#define CTRL_IO_KIND_LEN        16U
#define CTRL_RESOURCE_REF_LEN     32U
#define CTRL_SESSION_REF_LEN    64U
#define CTRL_REASON_CODE_LEN    32U
#define CTRL_PROMPT_CODE_LEN    40U

typedef struct {
    char imei[CTRL_IMEI_LEN];
    char iccid[CTRL_ICCID_LEN];
    char hardware_sku[CTRL_HW_SKU_LEN];
    char hardware_rev[CTRL_HW_REV_LEN];
    char firmware_family[CTRL_FW_FAMILY_LEN];
    char firmware_version[CTRL_FW_VERSION_LEN];
} controller_identity_t;

typedef struct {
    uint8_t relay_output;
    uint8_t motor_driver;
    uint8_t digital_input;
    uint8_t analog_input;
    uint8_t pulse_input;
    uint8_t rs485_modbus;
    uint8_t power_monitor;
    uint8_t card_reader;
} resource_inventory_t;

typedef struct {
    uint8_t payment_qr_control;
    uint8_t card_auth_reader;
    uint8_t electric_meter_modbus;
    uint8_t breaker_control;
    uint8_t breaker_feedback_monitor;
    uint8_t power_monitoring;
    uint8_t pressure_acquisition;
    uint8_t flow_acquisition;
    uint8_t level_acquisition;
    uint8_t soil_moisture_acquisition;
    uint8_t soil_temperature_acquisition;
    uint8_t pump_vfd_control;
    uint8_t single_valve_control;
    uint8_t pump_direct_control;
    uint8_t rs485_sensor_gateway;
    uint8_t rs485_vfd_gateway;
    uint8_t valve_feedback_monitor;
    uint8_t pump_fault_feedback;
    uint8_t remote_start_enable;
    uint8_t auto_linkage_enable;
    uint8_t auto_stop_on_low_pressure;
    uint8_t auto_stop_on_high_pressure;
} feature_modules_t;

typedef struct {
    char     channel_code[CTRL_CHANNEL_CODE_LEN];
    char     module_code[CTRL_MODULE_CODE_LEN];
    char     channel_role[CTRL_CHANNEL_ROLE_LEN];
    char     io_kind[CTRL_IO_KIND_LEN];
    char     resource_ref[CTRL_RESOURCE_REF_LEN];
    uint8_t  enabled;
} channel_binding_t;

typedef struct {
    /** 兼容旧配置：>0 且 link_ping_interval_sec==0 时，按此间隔发「完整体征」心跳。 */
    uint16_t heartbeat_interval_sec;
    /** 轻量保活心跳间隔（秒）。与 vitals 分离可显著省流量；0 表示不启用链路 ping。 */
    uint16_t link_ping_interval_sec;
    /** 完整体征（4G/电池等）上报间隔（秒）；0 表示仅靠变化触发+快照。 */
    uint16_t vitals_interval_sec;
    /** |CSQ 变化| ≥ 此值则立即补发一条 vitals；0 关闭。 */
    uint8_t  vitals_csq_delta;
    /** |SOC 变化| ≥ 此值则立即补发 vitals；0 关闭。 */
    uint8_t  vitals_soc_delta;
    uint16_t snapshot_idle_interval_sec;
    uint16_t snapshot_running_interval_sec;
    uint16_t runtime_tick_interval_sec;
    uint16_t ready_grace_sec;
    uint16_t valve_action_timeout_sec;
    uint16_t vfd_action_timeout_sec;
    uint8_t  workflow_enabled;
    uint32_t cloud_auth_timeout_ms;
} runtime_rules_t;

typedef enum {
    WF_BOOTING = 0,
    WF_ONLINE_NOT_READY = 1,
    WF_READY_IDLE = 2,
    WF_STARTING = 3,
    WF_RUNNING = 4,
    WF_PAUSING = 5,
    WF_PAUSED = 6,
    WF_RESUMING = 7,
    WF_STOPPING = 8,
    WF_STOPPED = 9,
    WF_ERROR_STOP = 10
} workflow_state_t;

typedef enum {
    SESSION_STATE_BOOTING = 0,
    SESSION_STATE_NOT_READY,
    SESSION_STATE_READY_IDLE,
    SESSION_STATE_AUTH_PENDING,
    SESSION_STATE_PRE_START_METERING,
    SESSION_STATE_START_SEQUENCE,
    SESSION_STATE_RUNNING,
    SESSION_STATE_STOP_SEQUENCE,
    SESSION_STATE_POST_STOP_METERING,
    SESSION_STATE_BLOCKED,
    SESSION_STATE_FAULT_LATCHED,
    SESSION_STATE_RECOVERY_LOCKED
} session_main_state_t;

typedef enum {
    CARD_STATE_IDLE = 0,
    CARD_STATE_READ,
    CARD_STATE_DEBOUNCE,
    CARD_STATE_READY_CHECK,
    CARD_STATE_VOICE_PROMPT,
    CARD_STATE_CLOUD_AUTH_PENDING,
    CARD_STATE_AUTH_GRANTED,
    CARD_STATE_AUTH_DENIED,
    CARD_STATE_STARTING,
    CARD_STATE_COMPLETED,
    CARD_STATE_BLOCKED
} card_flow_state_t;

typedef enum {
    SAFE_STATE_NORMAL = 0,
    SAFE_STATE_REJECT_START,
    SAFE_STATE_STOPPING,
    SAFE_STATE_LATCHED
} safety_state_t;

typedef enum {
    EVT_BOOT_OK = 0,
    EVT_BOOT_FAIL,
    EVT_SCAN_START_REQUEST,
    EVT_CARD_SWIPED,
    EVT_READY_CHECK_OK,
    EVT_READY_CHECK_FAIL,
    EVT_CLOUD_AUTH_GRANTED,
    EVT_CLOUD_AUTH_DENIED,
    EVT_PRE_START_METERING_OK,
    EVT_PRE_START_METERING_FAIL,
    EVT_START_SEQUENCE_OK,
    EVT_START_SEQUENCE_FAIL,
    EVT_RUNNING_TICK,
    EVT_STOP_REQUEST,
    EVT_STOP_SEQUENCE_OK,
    EVT_POST_STOP_METERING_OK,
    EVT_POST_STOP_METERING_FAIL,
    EVT_PROTECTION_TRIGGERED,
    EVT_NETWORK_LOST,
    EVT_NETWORK_RECOVERED,
    EVT_PUMP_POWER_LOST,
    EVT_CONTROLLER_POWER_RESTORED,
    EVT_FULL_POWER_RESTORE_BOOT,
    EVT_SESSION_LEASE_EXPIRED,
    EVT_CONFIG_UPDATED
} runtime_event_t;

typedef enum {
    BLOCKED_NONE = 0,
    BLOCKED_NOT_READY_BOOTING,
    BLOCKED_NOT_READY_CONFIG,
    BLOCKED_PROTECTION_LATCHED,
    BLOCKED_ALREADY_RUNNING,
    BLOCKED_RESOURCE_BUSY,
    BLOCKED_POWER_ABNORMAL,
    BLOCKED_OVER_VOLTAGE,
    BLOCKED_UNDER_VOLTAGE,
    BLOCKED_PHASE_LOSS,
    BLOCKED_OVER_CURRENT_LOCKED,
    BLOCKED_PRESSURE_ABNORMAL,
    BLOCKED_DRY_RUN_RISK,
    BLOCKED_NETWORK_UNAVAILABLE,
    BLOCKED_METER_UNAVAILABLE,
    BLOCKED_FLOW_SENSOR_UNAVAILABLE,
    BLOCKED_RECOVERY_LOCKED,
    BLOCKED_SESSION_LEASE_INVALID
} blocked_reason_code_t;

typedef enum {
    VOICE_DEVICE_BOOTING = 0,
    VOICE_DEVICE_NOT_READY,
    VOICE_DEVICE_CONFIG_MISSING,
    VOICE_NETWORK_UNAVAILABLE,
    VOICE_AUTH_CHECKING,
    VOICE_AUTH_GRANTED,
    VOICE_AUTH_DENIED,
    VOICE_METER_QUERY_FAILED,
    VOICE_STARTING_PUMP,
    VOICE_STARTING_VALVE,
    VOICE_START_SUCCESS,
    VOICE_START_FAILED,
    VOICE_ALREADY_RUNNING,
    VOICE_STOPPING,
    VOICE_STOPPED,
    VOICE_PROTECTION_TRIGGERED,
    VOICE_POWER_INTERRUPTED,
    VOICE_POWER_RESTORED_SELF_CHECK,
    VOICE_RECOVERY_LOCKED_CONFIRM_REQUIRED
} voice_prompt_code_t;

typedef enum {
    LINKAGE_LOCAL_INTEGRATED = 0,
    LINKAGE_PLATFORM_ORCHESTRATED
} linkage_mode_t;

typedef enum {
    TARGET_PUMP = 0,
    TARGET_VALVE,
    TARGET_PUMP_WITH_VALVE
} session_target_type_t;

typedef enum {
    SETTLEMENT_BY_DURATION_HOUR = 0,
    SETTLEMENT_BY_ENERGY_KWH,
    SETTLEMENT_BY_WATER_M3
} settlement_mode_t;

typedef enum {
    STOP_REASON_NONE = 0,
    STOP_REASON_PLATFORM_STOP,
    STOP_REASON_LOCAL_STOP,
    STOP_REASON_PROTECTION_TRIP,
    STOP_REASON_SESSION_LEASE_EXPIRED,
    STOP_REASON_PUMP_POWER_LOSS,
    STOP_REASON_CONTROLLER_POWER_INTERRUPT
} session_stop_reason_t;

typedef enum {
    PUMP_CONTROL_RELAY_DIRECT = 0,
    PUMP_CONTROL_METER_BREAKER_485,
    PUMP_CONTROL_CONTACTOR_DIRECT
} pump_control_mode_t;

typedef enum {
    VALVE_CONTROL_RELAY_OUTPUT = 0,
    VALVE_CONTROL_DIRECT_OUTPUT
} valve_control_mode_t;

typedef enum {
    PUMP_FAIL_SAFE_DEENERGIZE = 0,
    PUMP_FAIL_SAFE_HOLD_LAST
} pump_output_fail_safe_t;

typedef enum {
    VALVE_FAIL_CLOSE = 0,
    VALVE_FAIL_OPEN,
    VALVE_HOLD_LAST
} valve_fail_safe_mode_t;

typedef enum {
    POWER_DOMAIN_UNKNOWN = 0,
    POWER_DOMAIN_OFF,
    POWER_DOMAIN_ON,
    POWER_DOMAIN_LOST
} power_domain_status_t;

typedef enum {
    PHASE_STATUS_UNKNOWN = 0,
    PHASE_STATUS_OK,
    PHASE_STATUS_LOSS
} phase_status_t;

/** Spec §8.1 — module table ops (cfg/out payloads are module-specific; cast at boundary). */
typedef struct {
    const char *module_code;
    void (*init)(void);
    void (*tick_100ms)(void);
    void (*tick_1s)(void);
    uint8_t (*apply_config)(const void *cfg);
    uint8_t (*query_state)(void *out);
    uint8_t (*query_values)(void *out);
    uint8_t (*execute_action)(const char *action_code, const char *target_ref, const void *payload);
} module_ops_t;

#endif /* MODEL_TYPES_H */
