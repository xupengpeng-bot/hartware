#include "app_main.h"

#include "app_health.h"
#include "app_scheduler.h"
#include "config_store.h"
#include "runtime_state.h"
#include "safety_flow.h"
#include "telemetry.h"

#include "proto_command.h"
#include "proto_codec_json.h"
#include "proto_dispatch.h"
#include "proto_execute_action.h"
#include "proto_heartbeat.h"
#include "proto_ota.h"
#include "proto_query.h"
#include "proto_register.h"
#include "proto_state_snapshot.h"
#include "proto_sync_config.h"

#include "common_alarm.h"
#include "boot_diag.h"
#include "common_identity.h"
#include "common_status.h"

#include "module_registry.h"

#include "net_connectivity.h"
#include "net_platform_config.h"

#include "storage_recovery.h"
#include "storage_runtime.h"

#include "workflow_card_reader.h"
#include "workflow_engine.h"
#include "workflow_local_access.h"
#include "workflow_voice.h"

#include "bsp_adc.h"
#include "bsp_rtc.h"
#include "bsp_status_led.h"
#include "bsp_uart.h"

#include "ota_port_board.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int cb_register_build(char *buf, size_t cap, void *user)
{
    (void)user;
    return telemetry_build_register(buf, cap);
}

static int cb_heartbeat_build(char *buf, size_t cap, void *user)
{
    (void)user;
    return telemetry_build_heartbeat(buf, cap);
}

static int cb_snapshot_build(char *buf, size_t cap, void *user)
{
    (void)user;
    return telemetry_build_state_snapshot(buf, cap);
}

static int cb_sync_config(const char *json, size_t len, char *reply, size_t reply_cap, void *user)
{
    char corr[48];
    char session_ref[64];
    char extra[96];
    int rc;

    (void)user;

    corr[0] = '\0';
    session_ref[0] = '\0';
    (void)proto_json_get_string(json, "c", corr, sizeof(corr));
    (void)proto_json_get_string(json, "r", session_ref, sizeof(session_ref));

    rc = proto_sync_config_apply(json, len, extra, sizeof(extra));
    if (rc != 0) {
        return proto_build_command_nack(reply, reply_cap,
                                        corr[0] != '\0' ? corr : NULL,
                                        session_ref[0] != '\0' ? session_ref : NULL,
                                        corr[0] != '\0' ? corr : "sync_config",
                                        "SYNC_CONFIG",
                                        rc == -3 ? "CONFIG_VERSION_MISMATCH" : (rc == -6 ? "DEVICE_BUSY" : "PARAM_INVALID"),
                                        "sync_config_failed",
                                        NULL);
    }

    {
        const device_config_t *cfg = config_store_active();
        if (cfg != NULL) {
            common_status_set_config_version(cfg->config_version);
            runtime_state_set_config_version(cfg->config_version);
        }
    }
    net_platform_config_reload_from_device_config();
    net_connectivity_force_reconnect();
    return proto_build_command_ack(reply, reply_cap,
                                   corr[0] != '\0' ? corr : NULL,
                                   session_ref[0] != '\0' ? session_ref : NULL,
                                   corr[0] != '\0' ? corr : "sync_config",
                                   "SYNC_CONFIG",
                                   "controller",
                                   "accepted",
                                   extra);
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

static void cb_command_ack(const char *json, size_t len, void *user)
{
    (void)json;
    (void)len;
    (void)user;
    bsp_debug_log("[PROTO] downlink COMMAND_ACK received\r\n");
}

static void cb_query_result(const char *json, size_t len, void *user)
{
    (void)len;
    (void)user;
    safety_flow_on_query_result(json);
    bsp_debug_log("[PROTO] downlink QUERY_RESULT routed to safety_flow\r\n");
}

static void cb_register_ack(void *user)
{
    (void)user;
    net_connectivity_on_register_ack();
}

static void cb_register_nack(void *user)
{
    (void)user;
    net_connectivity_on_register_nack();
}

static const proto_dispatch_handlers_t s_proto_handlers = {
    .on_register_build       = cb_register_build,
    .on_heartbeat_build      = cb_heartbeat_build,
    .on_state_snapshot_build = cb_snapshot_build,
    .on_sync_config          = cb_sync_config,
    .on_query                = cb_query,
    .on_execute_action       = cb_execute,
    .on_command_ack          = cb_command_ack,
    .on_query_result         = cb_query_result,
    .on_register_ack         = cb_register_ack,
    .on_register_nack        = cb_register_nack,
    .user                    = NULL,
};

void app_main_init(void)
{
    ota_upgrade_capability_t cap;
    controller_identity_t *id;
    resource_inventory_t *ri;
    const device_config_t *cfg;

    bsp_debug_log("[INIT] runtime/config/status init\r\n");
    runtime_state_init();
    config_store_init();
    config_store_seed_defaults();
    storage_runtime_init();
    storage_recovery_init();
    common_identity_init();
    common_status_init();
    common_status_set_reboot_reason(boot_diag_reset_reason_code());
    common_alarm_init();
    bsp_adc_init();
    common_status_refresh_slow();
    safety_flow_init();

    cfg = config_store_active();
    if (cfg != NULL) {
        common_status_set_config_version(cfg->config_version);
        runtime_state_set_config_version(cfg->config_version);
    }

    ri = common_resource_inventory_mutable();
    ri->relay_output = 0U;
    ri->motor_driver = 0U;
    ri->digital_input = 2U;
    ri->analog_input = 2U;
    ri->pulse_input = 1U;
    ri->rs485_modbus = 1U;
    ri->power_monitor = 1U;
    ri->card_reader = 1U;

    id = common_identity_mutable();

    net_platform_config_reload_from_device_config();

    bsp_debug_log("[INIT] modules/workflows init\r\n");
    module_registry_init();
    module_registry_register_builtin();
    module_registry_init_all();
    workflow_engine_init();
    workflow_card_reader_init();
    workflow_local_access_init();
    workflow_voice_init();

    bsp_debug_log("[INIT] transport/protocol init\r\n");
    net_connectivity_init();
    proto_dispatch_init(&s_proto_handlers);

    memset(&cap, 0, sizeof(cap));
    cap.ota_supported = true;
    cap.dual_bank = false;
    cap.min_battery_soc_default = 30U;
    cap.min_signal_csq_default = 8;
    (void)strncpy(cap.package_formats, "raw-bin", sizeof(cap.package_formats) - 1U);
    (void)strncpy(cap.compression_formats, "none", sizeof(cap.compression_formats) - 1U);
    proto_ota_init(ota_port_board(), id->firmware_version, &cap, NULL, NULL);

    app_health_init();
    app_scheduler_init();

    bsp_debug_log("[INIT] REGISTER will be sent after TCP connect\r\n");
    bsp_debug_log("[INIT] DONE\r\n");
}

void app_main_loop_iteration(uint32_t monotonic_ms)
{
    bsp_rtc_tick(monotonic_ms);
    app_health_poll();
    app_scheduler_tick(monotonic_ms);
    net_connectivity_poll(monotonic_ms);
    bsp_status_led_poll(monotonic_ms);
    proto_ota_poll();
}
