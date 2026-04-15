#include "config_store.h"

#include "common_status.h"
#include "fw_build_config.h"
#include "scan_trial_defs.h"
#include "storage_config.h"

#include <string.h>

static void config_store_fill_defaults(device_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->config_version = 1U;

    cfg->feature_modules.payment_qr_control = 1U;
    cfg->feature_modules.card_auth_reader = 1U;
    cfg->feature_modules.electric_meter_modbus = 1U;
    cfg->feature_modules.breaker_control = 1U;

    cfg->runtime_rules.heartbeat_interval_sec = 30U;
    cfg->runtime_rules.snapshot_idle_interval_sec = 300U;
    cfg->runtime_rules.snapshot_running_interval_sec = 30U;
    cfg->runtime_rules.cloud_auth_timeout_ms = 8000U;
    cfg->runtime_rules.workflow_enabled = 1U;

    cfg->protection_config.overload_protection = 1U;
    cfg->protection_config.phase_loss_protection = 1U;
    cfg->protection_config.under_voltage_protection = 1U;
    cfg->protection_config.over_voltage_protection = 1U;
    cfg->protection_config.dry_run_protection = 0U;
    cfg->protection_config.pressure_high_limit = 0.0f;
    cfg->protection_config.pressure_low_limit = 0.0f;
    cfg->protection_config.start_delay_ms = 1500U;
    cfg->protection_config.stop_delay_ms = 1500U;

    cfg->control_config.pump_control_mode = PUMP_CONTROL_METER_BREAKER_485;
    cfg->control_config.valve_control_mode = VALVE_CONTROL_DIRECT_OUTPUT;
    cfg->control_config.pump_output_fail_safe = PUMP_FAIL_SAFE_DEENERGIZE;
    cfg->control_config.valve_fail_safe_mode = VALVE_FAIL_CLOSE;
    cfg->control_config.linkage_mode = LINKAGE_LOCAL_INTEGRATED;
    cfg->control_config.offline_new_start_enabled = 0U;
    cfg->control_config.power_restore_resume_enabled = 0U;
    cfg->control_config.cross_device_linkage_from_edge = 0U;
    cfg->control_config.offline_max_runtime_sec = 300U;

    (void)strncpy(cfg->platform_tcp_host, FW_PLATFORM_TCP_HOST, sizeof(cfg->platform_tcp_host) - 1U);
    cfg->platform_tcp_port = FW_PLATFORM_TCP_PORT;
    cfg->time_zone_quarter_hours = MODEL_DEFAULT_TIME_ZONE_QUARTER_HOURS;

    cfg->channel_binding_count = 3U;
    (void)strncpy(cfg->channel_bindings[0].channel_code, "pump_1", sizeof(cfg->channel_bindings[0].channel_code) - 1U);
    (void)strncpy(cfg->channel_bindings[0].module_code, "breaker_control", sizeof(cfg->channel_bindings[0].module_code) - 1U);
    (void)strncpy(cfg->channel_bindings[0].channel_role, "pump_run", sizeof(cfg->channel_bindings[0].channel_role) - 1U);
    (void)strncpy(cfg->channel_bindings[0].io_kind, "rs485", sizeof(cfg->channel_bindings[0].io_kind) - 1U);
    (void)strncpy(cfg->channel_bindings[0].resource_ref, "rs485_meter_1", sizeof(cfg->channel_bindings[0].resource_ref) - 1U);
    cfg->channel_bindings[0].enabled = 1U;

    (void)strncpy(cfg->channel_bindings[1].channel_code, "card_reader_1", sizeof(cfg->channel_bindings[1].channel_code) - 1U);
    (void)strncpy(cfg->channel_bindings[1].module_code, "card_auth_reader", sizeof(cfg->channel_bindings[1].module_code) - 1U);
    (void)strncpy(cfg->channel_bindings[1].channel_role, "card_reader", sizeof(cfg->channel_bindings[1].channel_role) - 1U);
    (void)strncpy(cfg->channel_bindings[1].io_kind, "uart", sizeof(cfg->channel_bindings[1].io_kind) - 1U);
    (void)strncpy(cfg->channel_bindings[1].resource_ref, "uart_card_reader", sizeof(cfg->channel_bindings[1].resource_ref) - 1U);
    cfg->channel_bindings[1].enabled = 1U;

    (void)strncpy(cfg->channel_bindings[2].channel_code, "meter_1", sizeof(cfg->channel_bindings[2].channel_code) - 1U);
    (void)strncpy(cfg->channel_bindings[2].module_code, "electric_meter_modbus", sizeof(cfg->channel_bindings[2].module_code) - 1U);
    (void)strncpy(cfg->channel_bindings[2].channel_role, "electric_meter", sizeof(cfg->channel_bindings[2].channel_role) - 1U);
    (void)strncpy(cfg->channel_bindings[2].io_kind, "rs485", sizeof(cfg->channel_bindings[2].io_kind) - 1U);
    (void)strncpy(cfg->channel_bindings[2].resource_ref, "rs485_meter_1", sizeof(cfg->channel_bindings[2].resource_ref) - 1U);
    cfg->channel_bindings[2].enabled = 1U;

    (void)SCAN_TRIAL_CONTROLLER_NAME;
}

void config_store_init(void)
{
    storage_config_init();
}

void config_store_seed_defaults(void)
{
    device_config_t *cfg = storage_config_inactive_mutable();
    if (cfg == NULL) {
        return;
    }
    config_store_fill_defaults(cfg);
    (void)storage_config_commit_swap(cfg->config_version);
    common_status_set_config_version(cfg->config_version);
}

const device_config_t *config_store_active(void)
{
    return storage_config_active();
}

uint32_t config_store_config_version(void)
{
    const device_config_t *cfg = storage_config_active();
    return cfg != NULL ? cfg->config_version : 0U;
}

const protection_config_t *config_store_protection(void)
{
    const device_config_t *cfg = storage_config_active();
    return cfg != NULL ? &cfg->protection_config : NULL;
}

const control_config_t *config_store_control(void)
{
    const device_config_t *cfg = storage_config_active();
    return cfg != NULL ? &cfg->control_config : NULL;
}
