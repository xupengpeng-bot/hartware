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

typedef struct {
    char imei[CTRL_IMEI_LEN];
    char iccid[CTRL_ICCID_LEN];
    char hardware_sku[CTRL_HW_SKU_LEN];
    char hardware_rev[CTRL_HW_REV_LEN];
    char firmware_family[CTRL_FW_FAMILY_LEN];
    char firmware_version[CTRL_FW_VERSION_LEN];
} controller_identity_t;

typedef struct {
    uint8_t ai_count;
    uint8_t di_count;
    uint8_t do_count;
    uint8_t rs485_count;
    uint8_t relay_count;
    uint8_t pulse_count;
    uint8_t battery_monitor;
    uint8_t solar_monitor;
    uint8_t signal_monitor;
} resource_inventory_t;

typedef struct {
    uint8_t pump_vfd_control;
    uint8_t single_valve_control;
    uint8_t pressure_acquisition;
    uint8_t flow_acquisition;
    uint8_t electric_meter_modbus;
    uint8_t soil_moisture_acquisition;
    uint8_t soil_temperature_acquisition;
    uint8_t liquid_level_acquisition;
    uint8_t remote_io_extension;
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
    uint16_t snapshot_interval_sec;
    uint16_t runtime_tick_interval_sec;
    uint16_t ready_grace_sec;
    uint16_t valve_action_timeout_sec;
    uint16_t vfd_action_timeout_sec;
    uint8_t  workflow_enabled;
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
