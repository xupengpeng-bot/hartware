#include "app_main.h"
#include "app_scheduler.h"
#include "app_health.h"
#include "proto_dispatch.h"
#include "proto_register.h"
#include "proto_heartbeat.h"
#include "proto_state_snapshot.h"
#include "proto_sync_config.h"
#include "proto_query.h"
#include "proto_execute_action.h"
#include "proto_command.h"
#include "proto_codec_json.h"
#include "storage_config.h"
#include "storage_runtime.h"
#include "storage_recovery.h"

#include "common_identity.h"
#include "common_status.h"
#include "common_alarm.h"

#include "module_registry.h"

#include "workflow_engine.h"
#include "workflow_card_reader.h"
#include "workflow_local_access.h"
#include "workflow_recovery.h"
#include "workflow_voice.h"

#include "net_connectivity.h"
#include "net_platform_config.h"
#include "bsp_adc.h"
#include "bsp_uart.h"

#include "proto_ota.h"
#include "ota_port_board.h"

#include <string.h>

static char s_proto_reg_buf[2048];
#include <stdio.h>

static int cb_register_build(char *buf, size_t cap, void *user)
{
    (void)user;
    return proto_register_build(buf, cap);
}

static int cb_heartbeat_build(char *buf, size_t cap, void *user)
{
    (void)user;
    return proto_heartbeat_build(buf, cap);
}

static int cb_snapshot_build(char *buf, size_t cap, void *user)
{
    (void)user;
    return proto_state_snapshot_build(buf, cap);
}

static int cb_sync_config(const char *json, size_t len, char *reply, size_t reply_cap, void *user)
{
    (void)user;
    char corr[48];
    char session_ref[64];
    corr[0] = '\0';
    session_ref[0] = '\0';
    (void)proto_json_get_string(json, "correlation_id", corr, sizeof(corr));
    (void)proto_json_get_string(json, "session_ref", session_ref, sizeof(session_ref));
    char extra[192];
    int  r = proto_sync_config_apply(json, len, extra, sizeof(extra));
    if (r != 0) {
        return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, r, "sync_config_failed");
    }
    device_config_t cfg;
    if (storage_config_load(&cfg) == 0) {
        common_status_set_config_version(cfg.config_version);
    }
    return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, extra);
}

static int cb_query(const char *json, size_t len, char *reply, size_t reply_cap, void *user)
{
    (void)user;
    return proto_query_handle(json, len, reply, reply_cap);
}

static int cb_execute(const char *json, size_t len, char *reply, size_t reply_cap, void *user)
{
    (void)user;
    return proto_execute_action_handle(json, len, reply, reply_cap);
}

static const proto_dispatch_handlers_t s_proto_handlers = {
    .on_register_build       = cb_register_build,
    .on_heartbeat_build      = cb_heartbeat_build,
    .on_state_snapshot_build = cb_snapshot_build,
    .on_sync_config          = cb_sync_config,
    .on_query                = cb_query,
    .on_execute_action       = cb_execute,
    .user                    = NULL,
};

static void app_seed_default_config(void)
{
    device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.config_version = 1U;
    cfg.feature_modules.pump_vfd_control         = 1U;
    cfg.feature_modules.pressure_acquisition     = 1U;
    cfg.feature_modules.flow_acquisition         = 1U;
    cfg.feature_modules.single_valve_control     = 1U;
    cfg.feature_modules.electric_meter_modbus    = 1U;
    cfg.feature_modules.soil_moisture_acquisition   = 1U;
    cfg.feature_modules.soil_temperature_acquisition = 1U;
    cfg.runtime_rules.heartbeat_interval_sec      = 0U;
    cfg.runtime_rules.link_ping_interval_sec      = 120U;
    cfg.runtime_rules.vitals_interval_sec         = 900U;
    cfg.runtime_rules.vitals_csq_delta            = 5U;
    cfg.runtime_rules.vitals_soc_delta            = 5U;
    cfg.runtime_rules.snapshot_interval_sec       = 600U;
    cfg.runtime_rules.runtime_tick_interval_sec = 1U;
    cfg.runtime_rules.workflow_enabled          = 1U;
    (void)storage_config_stage_inactive(&cfg);
    (void)storage_config_commit_swap(cfg.config_version);
}

void app_main_init(void)
{
    bsp_debug_log("[INIT] net_platform_config_init\r\n");
    net_platform_config_init();

    bsp_debug_log("[INIT] storage_config_init + seed\r\n");
    storage_config_init();
    app_seed_default_config();

    bsp_debug_log("[INIT] storage_runtime + recovery\r\n");
    storage_runtime_init();
    storage_recovery_init();

    bsp_debug_log("[INIT] identity status adc alarm\r\n");
    common_identity_init();
    common_status_init();
    bsp_adc_init();
    common_alarm_init();

    controller_identity_t *id = common_identity_mutable();
    (void)strncpy(id->imei, "864869000000001", sizeof(id->imei) - 1U);
    (void)strncpy(id->iccid, "89860000000000000000", sizeof(id->iccid) - 1U);

    resource_inventory_t *ri = common_resource_inventory_mutable();
    ri->ai_count         = 4U;
    ri->di_count         = 4U;
    ri->do_count         = 4U;
    ri->rs485_count      = 1U;
    ri->relay_count      = 2U;
    ri->pulse_count      = 1U;
    ri->battery_monitor  = 1U;
    ri->solar_monitor    = 1U;
    ri->signal_monitor   = 1U;

    bsp_debug_log("[INIT] module_registry\r\n");
    module_registry_init();
    module_registry_register_builtin();
    module_registry_init_all();

    bsp_debug_log("[INIT] workflows\r\n");
    workflow_engine_init();
    workflow_card_reader_init();
    workflow_local_access_init();
    workflow_voice_init();

    bsp_debug_log("[INIT] net_connectivity + proto_dispatch\r\n");
    net_connectivity_init();
    proto_dispatch_init(&s_proto_handlers);

    ota_upgrade_capability_t cap;
    memset(&cap, 0, sizeof(cap));
    cap.ota_supported            = true;
    cap.dual_bank                = false;
    cap.min_battery_soc_default  = 30U;
    cap.min_signal_csq_default   = 8;
    (void)strncpy(cap.package_formats, "raw-bin", sizeof(cap.package_formats) - 1U);
    (void)strncpy(cap.compression_formats, "none", sizeof(cap.compression_formats) - 1U);

    bsp_debug_log("[INIT] proto_ota\r\n");
    proto_ota_init(ota_port_board(), id->firmware_version, &cap, NULL, NULL);

    bsp_debug_log("[INIT] workflow_recovery_boot_check\r\n");
    workflow_recovery_boot_check();

    bsp_debug_log("[INIT] app_health + scheduler\r\n");
    app_health_init();
    app_scheduler_init();

    int n = proto_register_build(s_proto_reg_buf, sizeof(s_proto_reg_buf));
    bsp_debug_log("[INIT] proto_register_build\r\n");
    if (n > 0) {
        (void)net_connectivity_send_json(s_proto_reg_buf, (size_t)n);
    }
    common_status_set_registered(true);
    bsp_debug_log("[INIT] DONE (registered=true)\r\n");
}

void app_main_loop_iteration(uint32_t monotonic_ms)
{
    app_health_poll();
    app_scheduler_tick(monotonic_ms);
    net_connectivity_poll(monotonic_ms);
    proto_ota_poll();
}
