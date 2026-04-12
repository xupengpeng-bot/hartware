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
#include "proto_event_report.h"

#include "common_alarm.h"
#include "boot_diag.h"
#include "common_identity.h"
#include "common_status.h"

#include "module_registry.h"

#include "net_connectivity.h"
#include "net_platform_config.h"

#include "storage_recovery.h"
#include "storage_runtime.h"
#include "storage_upgrade.h"

#include "workflow_card_reader.h"
#include "workflow_engine.h"
#include "workflow_local_access.h"
#include "workflow_voice.h"

#include "bsp_adc.h"
#include "bsp_rtc.h"
#include "bsp_status_led.h"
#include "bsp_uart.h"

#include "boot_control.h"
#include "ota_port_board.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t pending;
    char upgrade_token[OTA_TICKET_MAX];
    char upgrade_job_id[OTA_ID_MAX];
    char upgrade_item_id[OTA_ID_MAX];
    char release_id[OTA_ID_MAX];
    char release_code[OTA_VERSION_STRING_MAX];
    char package_artifact_id[OTA_ID_MAX];
    char stage[24];
    char result[16];
    uint8_t progress_percent;
    char reason_code[24];
    char message[OTA_ERROR_MESSAGE_MAX];
    char firmware_version[CTRL_FW_VERSION_LEN];
    char checksum[OTA_SHA256_HEX_LEN];
} ota_report_pending_t;

static ota_report_pending_t s_ota_report;

static void ota_copy_text(char *dst, size_t cap, const char *src)
{
    if (dst == NULL || cap == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, cap, "%s", src);
}

static void ota_report_fill_meta(ota_report_pending_t *report)
{
    ota_prepare_payload_t manifest;
    const controller_identity_t *id = common_identity_get();

    if (report == NULL) {
        return;
    }
    memset(&manifest, 0, sizeof(manifest));
    if (storage_upgrade_load_manifest(&manifest) == 0) {
        ota_copy_text(report->upgrade_token, sizeof(report->upgrade_token), manifest.upgrade_ticket);
        ota_copy_text(report->upgrade_job_id, sizeof(report->upgrade_job_id), manifest.upgrade_job_id);
        ota_copy_text(report->upgrade_item_id, sizeof(report->upgrade_item_id), manifest.upgrade_item_id);
        ota_copy_text(report->release_id, sizeof(report->release_id), manifest.release_id);
        ota_copy_text(report->release_code, sizeof(report->release_code), manifest.release_code);
        ota_copy_text(report->package_artifact_id, sizeof(report->package_artifact_id), manifest.package_artifact_id);
        ota_copy_text(report->checksum, sizeof(report->checksum), manifest.package_sha256_hex);
    }
    if (id != NULL) {
        ota_copy_text(report->firmware_version, sizeof(report->firmware_version), id->firmware_version);
    }
}

static void ota_report_queue_and_try_send(const ota_report_pending_t *report)
{
    int rc;

    if (report == NULL || report->upgrade_token[0] == '\0') {
        return;
    }
    rc = proto_event_report_send_upgrade_report(report->upgrade_token,
                                                report->upgrade_job_id,
                                                report->upgrade_item_id,
                                                report->release_id,
                                                report->release_code,
                                                report->package_artifact_id,
                                                report->stage,
                                                report->result,
                                                report->progress_percent,
                                                report->reason_code[0] != '\0' ? report->reason_code : NULL,
                                                report->message[0] != '\0' ? report->message : NULL,
                                                report->firmware_version[0] != '\0' ? report->firmware_version : NULL,
                                                report->checksum[0] != '\0' ? report->checksum : NULL);
    if (rc > 0) {
        memset(&s_ota_report, 0, sizeof(s_ota_report));
    } else {
        s_ota_report = *report;
        s_ota_report.pending = 1U;
    }
}

static void ota_report_try_emit_pending(void)
{
    ota_report_pending_t pending;

    if (s_ota_report.pending == 0U) {
        return;
    }
    pending = s_ota_report;
    ota_report_queue_and_try_send(&pending);
}

static void cb_ota_event(const proto_ota_event_t *event, void *user)
{
    ota_report_pending_t report;

    (void)user;
    if (event == NULL) {
        return;
    }

    memset(&report, 0, sizeof(report));
    ota_report_fill_meta(&report);

    switch (event->code) {
    case OTA_EVENT_OTA_COMMAND_ACKED:
        ota_copy_text(report.stage, sizeof(report.stage), "command_acked");
        ota_copy_text(report.result, sizeof(report.result), "accepted");
        report.progress_percent = 0U;
        break;
    case OTA_EVENT_OTA_DOWNLOAD_PROGRESS:
        ota_copy_text(report.stage, sizeof(report.stage), "downloading");
        ota_copy_text(report.result, sizeof(report.result), "running");
        report.progress_percent = event->u.download_progress.download_progress_pct;
        break;
    case OTA_EVENT_OTA_WRITE_COMPLETED:
    case OTA_EVENT_OTA_VERIFY_PASSED:
        ota_copy_text(report.stage, sizeof(report.stage), "installing");
        ota_copy_text(report.result, sizeof(report.result), "running");
        report.progress_percent = 100U;
        break;
    case OTA_EVENT_OTA_SWITCH_SCHEDULED:
        ota_copy_text(report.stage, sizeof(report.stage), "rebooting");
        ota_copy_text(report.result, sizeof(report.result), "running");
        report.progress_percent = 100U;
        break;
    case OTA_EVENT_OTA_UPGRADE_SUCCEEDED:
        net_connectivity_resume_after_ota();
        ota_copy_text(report.stage, sizeof(report.stage), "succeeded");
        ota_copy_text(report.result, sizeof(report.result), "succeeded");
        report.progress_percent = 100U;
        break;
    case OTA_EVENT_OTA_UPGRADE_FAILED:
        net_connectivity_resume_after_ota();
        ota_copy_text(report.stage, sizeof(report.stage), "failed");
        ota_copy_text(report.result, sizeof(report.result), "failed");
        ota_copy_text(report.reason_code, sizeof(report.reason_code), "exec_failed");
        (void)snprintf(report.message, sizeof(report.message), "upgrade failed(%ld)",
                       (long)event->u.error_code);
        break;
    default:
        return;
    }

    ota_report_queue_and_try_send(&report);
}

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
    const ota_port_t *ota_port;
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
    memset(&s_ota_report, 0, sizeof(s_ota_report));

    ota_port = ota_port_board();

    memset(&cap, 0, sizeof(cap));
    cap.ota_supported = (ota_port != NULL &&
                         ota_port->http_download_chunk != NULL &&
                         ota_port->flash_write_upgrade_region != NULL &&
                         ota_port->sha256_init != NULL &&
                         ota_port->sha256_update != NULL &&
                         ota_port->sha256_final != NULL &&
                         ota_port->sha256_free != NULL &&
                         ota_port->reboot_to_new_image != NULL);
    cap.dual_bank = false;
    cap.min_battery_soc_default = 30U;
    cap.min_signal_csq_default = 8;
    (void)strncpy(cap.package_formats, "raw-bin", sizeof(cap.package_formats) - 1U);
    (void)strncpy(cap.compression_formats, "none", sizeof(cap.compression_formats) - 1U);
    proto_ota_init(ota_port, id->firmware_version, &cap, cb_ota_event, NULL);
    {
        boot_control_record_t boot_record;
        if (boot_control_load(&boot_record) == 0 &&
            boot_record.state == BOOT_CONTROL_STATE_TRIAL_BOOT) {
            proto_ota_report_upgrade_succeeded_after_boot();
            (void)boot_control_clear();
        }
    }

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
    ota_report_try_emit_pending();
}
