#include "proto_execute_action.h"

#include "config_store.h"
#include "ota_port_board.h"
#include "proto_ota.h"
#include "proto_codec_json.h"
#include "proto_command.h"
#include "runtime_state.h"
#include "safety_flow.h"
#include "workflow_control.h"
#include "module_registry.h"

#include "bsp_uart.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>

#ifndef FW_PROTO_VERBOSE_LOG
#define FW_PROTO_VERBOSE_LOG 1
#endif

#if !FW_PROTO_VERBOSE_LOG
#define bsp_debug_log(...) ((void)0)
#endif

#define REMOTE_REBOOT_DELAY_MS 1000U
#define OTA_START_AFTER_ACK_MS 1000U

static uint8_t  s_remote_reboot_pending_ack;
static uint8_t  s_remote_reboot_armed;
static uint32_t s_remote_reboot_due_ms;
static uint8_t  s_ota_start_pending_ack;
static uint8_t  s_ota_start_armed;
static uint32_t s_ota_start_due_ms;

static int finalize_action_reply(char *reply, size_t reply_cap, const char *corr,
                                 const char *scope, const char *action,
                                 const char *target, int rc)
{
    (void)reply;
    (void)reply_cap;
    (void)corr;
    (void)scope;
    (void)action;
    (void)target;
    return rc;
}

static void log_json_parse_failure(const char *tag, const char *json, size_t json_len)
{
    char ascii[65];
    char hex[3 * 24 + 1];
    char line[256];
    const char *err_ptr;
    size_t offset = 0U;
    size_t dump_len;
    size_t start;
    size_t i;
    size_t ascii_len;
    size_t hex_pos = 0U;

    if (json == NULL) {
        return;
    }

    err_ptr = cJSON_GetErrorPtr();
    if (err_ptr != NULL && err_ptr >= json) {
        size_t candidate = (size_t)(err_ptr - json);
        if (candidate <= json_len) {
            offset = candidate;
        }
    }

    start = offset > 12U ? (offset - 12U) : 0U;
    dump_len = json_len - start;
    if (dump_len > 24U) {
        dump_len = 24U;
    }

    ascii_len = dump_len > (sizeof(ascii) - 1U) ? (sizeof(ascii) - 1U) : dump_len;
    for (i = 0U; i < ascii_len; i++) {
        unsigned char ch = (unsigned char)json[start + i];
        ascii[i] = (ch >= 32U && ch <= 126U) ? (char)ch : '.';
    }
    ascii[ascii_len] = '\0';

    for (i = 0U; i < dump_len && (hex_pos + 3U) < sizeof(hex); i++) {
        unsigned char ch = (unsigned char)json[start + i];
        int wrote = snprintf(hex + hex_pos, sizeof(hex) - hex_pos, "%02X", (unsigned)ch);
        if (wrote <= 0) {
            break;
        }
        hex_pos += (size_t)wrote;
        if (i + 1U < dump_len && hex_pos + 1U < sizeof(hex)) {
            hex[hex_pos++] = ' ';
        }
    }
    hex[hex_pos] = '\0';

    (void)snprintf(line, sizeof(line),
                   "[PROTO] %s invalid json offset=%lu len=%lu slice=\"%s\" hex=%s\r\n",
                   tag != NULL ? tag : "json",
                   (unsigned long)offset,
                   (unsigned long)json_len,
                   ascii,
                   hex);
    bsp_debug_log(line);
}

static void read_optional_string(const cJSON *obj, const char *key, char *out, size_t out_cap)
{
    const cJSON *item;

    if (out == NULL || out_cap == 0U) {
        return;
    }
    out[0] = '\0';
    if (obj == NULL || key == NULL) {
        return;
    }
    item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        (void)snprintf(out, out_cap, "%s", item->valuestring);
    }
}

static const char *map_ota_reject_code(int result, const ota_upgrade_status_t *status);
static const char *map_ota_reject_message(int result, const ota_upgrade_status_t *status);
static const char *map_ota_idempotent_stage(ota_state_t state);
static void ota_start_request_after_ack(void);

static const char *resolve_reply_session_ref(const char *explicit_session_ref, const char *detail_json)
{
    static char detail_session_ref[CTRL_SESSION_REF_LEN];
    const runtime_state_t *rs;

    if (explicit_session_ref != NULL && explicit_session_ref[0] != '\0') {
        return explicit_session_ref;
    }
    detail_session_ref[0] = '\0';
    if (detail_json != NULL && detail_json[0] != '\0' &&
        proto_json_get_string(detail_json, "session_ref", detail_session_ref, sizeof(detail_session_ref)) == 0 &&
        detail_session_ref[0] != '\0') {
        return detail_session_ref;
    }
    rs = runtime_state_get();
    if (rs != NULL && rs->session_ref[0] != '\0') {
        return rs->session_ref;
    }
    return NULL;
}

static int build_action_ack(char *reply, size_t reply_cap, const char *corr, const char *session_ref,
                            const char *scope, const char *action_code, const char *target_channel_code,
                            const char *extra_fields_json)
{
    const runtime_state_t *rs = runtime_state_get();
    char target_fragment[64];
    char local_extra[320];
    int wrote;

    target_fragment[0] = '\0';
    if (target_channel_code != NULL && target_channel_code[0] != '\0') {
        (void)snprintf(target_fragment, sizeof(target_fragment), ",\"tr\":\"%s\"", target_channel_code);
    }
    wrote = snprintf(local_extra, sizeof(local_extra),
                     "\"sc\":\"%s\",\"ac\":\"%s\",\"wf\":\"%s\"%s%s%s",
                     scope != NULL ? scope : "",
                     action_code != NULL ? action_code : "",
                     proto_map_workflow_short_from_runtime(
                         runtime_state_workflow_name(rs != NULL ? rs->workflow_state : RUNTIME_WORKFLOW_NOT_READY),
                         (rs != NULL && rs->ready) ? 1U : 0U),
                     target_fragment,
                     extra_fields_json != NULL && extra_fields_json[0] != '\0' ? "," : "",
                     extra_fields_json != NULL && extra_fields_json[0] != '\0' ? extra_fields_json : "");
    if (wrote < 0 || (size_t)wrote >= sizeof(local_extra)) {
        return -1;
    }
    return proto_build_command_ack(reply, reply_cap,
                                   corr != NULL && corr[0] != '\0' ? corr : NULL,
                                   resolve_reply_session_ref(session_ref, extra_fields_json),
                                   corr != NULL && corr[0] != '\0' ? corr : "action",
                                   "EXECUTE_ACTION",
                                   target_channel_code != NULL ? target_channel_code : "controller",
                                   "accepted",
                                   local_extra);
}

static int build_action_nack(char *reply, size_t reply_cap, const char *corr, const char *session_ref,
                             const char *scope, const char *action_code, const char *target_channel_code,
                             const char *reject_code, const char *reason)
{
    char extra[128];
    int wrote;

    wrote = snprintf(extra, sizeof(extra),
                     "\"sc\":\"%s\",\"ac\":\"%s\"%s%s%s",
                     scope != NULL ? scope : "",
                     action_code != NULL ? action_code : "",
                     target_channel_code != NULL && target_channel_code[0] != '\0' ? ",\"tr\":\"" : "",
                     target_channel_code != NULL && target_channel_code[0] != '\0' ? target_channel_code : "",
                     target_channel_code != NULL && target_channel_code[0] != '\0' ? "\"" : "");
    if (wrote < 0 || (size_t)wrote >= sizeof(extra)) {
        return -1;
    }
    return proto_build_command_nack(reply, reply_cap,
                                    corr != NULL && corr[0] != '\0' ? corr : NULL,
                                    resolve_reply_session_ref(session_ref, NULL),
                                    corr != NULL && corr[0] != '\0' ? corr : "action",
                                    "EXECUTE_ACTION",
                                    reject_code,
                                    reason,
                                    extra);
}

static const char *map_safety_reject_code(int result)
{
    const runtime_state_t *rs = runtime_state_get();
    blocked_reason_code_t blocked = rs != NULL ? rs->blocked_reason : BLOCKED_NONE;

    switch (result) {
    case -1:
        return "PARAM_INVALID";
    case -2:
        switch (blocked) {
        case BLOCKED_RESOURCE_BUSY:
        case BLOCKED_ALREADY_RUNNING:
            return "DEVICE_BUSY";
        case BLOCKED_POWER_ABNORMAL:
        case BLOCKED_OVER_VOLTAGE:
        case BLOCKED_UNDER_VOLTAGE:
        case BLOCKED_PHASE_LOSS:
            return "POWER_NOT_READY";
        case BLOCKED_PROTECTION_LATCHED:
        case BLOCKED_PRESSURE_ABNORMAL:
        case BLOCKED_DRY_RUN_RISK:
        case BLOCKED_RECOVERY_LOCKED:
        case BLOCKED_SESSION_LEASE_INVALID:
            return "SAFETY_INTERLOCK";
        case BLOCKED_METER_UNAVAILABLE:
        case BLOCKED_FLOW_SENSOR_UNAVAILABLE:
        case BLOCKED_NOT_READY_CONFIG:
            return "MODULE_NOT_ENABLED";
        default:
            return "DEVICE_BUSY";
        }
    case -3:
    case -8:
        return "DEVICE_BUSY";
    case -9:
        return "CAPABILITY_NOT_EXPOSED";
    default:
        return "CAPABILITY_NOT_EXPOSED";
    }
}

static int require_target(const char *target, const char *expected)
{
    return target != NULL && expected != NULL && strcmp(target, expected) == 0 ? 0 : -1;
}

static int require_target_or_empty(const char *target, const char *expected)
{
    if (target == NULL || target[0] == '\0') {
        return 0;
    }
    return require_target(target, expected);
}

static int require_valve_target(const char *target)
{
    if (target == NULL) {
        return -1;
    }
    return (strcmp(target, "valve_1") == 0 || strcmp(target, "valve_2") == 0) ? 0 : -1;
}

static int require_relay_target(const char *target)
{
    if (target == NULL) {
        return -1;
    }
    return (strcmp(target, "relay_1") == 0 || strcmp(target, "relay_2") == 0) ? 0 : -1;
}

static int execute_bound_valve_action(const device_config_t *cfg, const char *target, const char *action_code)
{
    const module_ops_t *ops = NULL;

    if (cfg == NULL || action_code == NULL || require_valve_target(target) != 0) {
        return -1;
    }
    if (cfg->feature_modules.dual_valve_control != 0U) {
        ops = module_registry_get("dual_valve_control");
    }
    if (ops == NULL || ops->execute_action == NULL) {
        return -2;
    }
    return ops->execute_action(action_code, target, NULL) == 0U ? 0 : -3;
}

static int execute_bound_relay_action(const device_config_t *cfg, const char *target, const char *action_code)
{
    const module_ops_t *ops;

    if (cfg == NULL || action_code == NULL || require_relay_target(target) != 0) {
        return -1;
    }
    if (cfg->feature_modules.relay_output_control == 0U) {
        return -2;
    }
    ops = module_registry_get("relay_output_control");
    if (ops == NULL || ops->execute_action == NULL) {
        return -2;
    }
    return ops->execute_action(action_code, target, NULL) == 0U ? 0 : -3;
}

static void read_optional_json_string_alias(const cJSON *obj,
                                            const char *key1,
                                            const char *key2,
                                            const char *key3,
                                            char *out,
                                            size_t out_cap)
{
    read_optional_string(obj, key1, out, out_cap);
    if (out[0] == '\0' && key2 != NULL) {
        read_optional_string(obj, key2, out, out_cap);
    }
    if (out[0] == '\0' && key3 != NULL) {
        read_optional_string(obj, key3, out, out_cap);
    }
}

static int parse_upgrade_prepare_payload(const cJSON *source, ota_prepare_payload_t *out)
{
    const ota_upgrade_capability_t *cap;
    uint32_t size_bytes = 0U;

    if (source == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    cap = proto_ota_get_capability();
    out->min_battery_soc = cap != NULL ? cap->min_battery_soc_default : 30U;
    out->min_signal_csq = cap != NULL ? cap->min_signal_csq_default : 8;
    out->auto_commit = true;

    read_optional_json_string_alias(source, "upgrade_token", "ut", "upgrade_ticket",
                                    out->upgrade_ticket, sizeof(out->upgrade_ticket));
    read_optional_json_string_alias(source, "upgrade_job_id", "uj", NULL,
                                    out->upgrade_job_id, sizeof(out->upgrade_job_id));
    read_optional_json_string_alias(source, "upgrade_item_id", "ui", NULL,
                                    out->upgrade_item_id, sizeof(out->upgrade_item_id));
    read_optional_json_string_alias(source, "release_id", "rid", NULL,
                                    out->release_id, sizeof(out->release_id));
    read_optional_json_string_alias(source, "release_code", "rcd", "target_version",
                                    out->release_code, sizeof(out->release_code));
    read_optional_json_string_alias(source, "package_artifact_id", "aid", NULL,
                                    out->package_artifact_id, sizeof(out->package_artifact_id));
    read_optional_json_string_alias(source, "package_download_url", "url", "package_url",
                                    out->package_url, sizeof(out->package_url));
    read_optional_json_string_alias(source, "package_file_name", "fn", NULL,
                                    out->package_file_name, sizeof(out->package_file_name));
    read_optional_json_string_alias(source, "package_checksum", "sum", "checksum",
                                    out->package_sha256_hex, sizeof(out->package_sha256_hex));
    read_optional_json_string_alias(source, "package_format", "fmt", NULL,
                                    out->package_format, sizeof(out->package_format));
    read_optional_json_string_alias(source, "package_etag", "etag", NULL,
                                    out->package_etag, sizeof(out->package_etag));
    if (out->package_format[0] == '\0') {
        (void)snprintf(out->package_format, sizeof(out->package_format), "raw-bin");
    }
    if (out->release_code[0] != '\0') {
        (void)snprintf(out->target_version, sizeof(out->target_version), "%s", out->release_code);
    }
    {
        cJSON *size_item = cJSON_GetObjectItemCaseSensitive((cJSON *)source, "package_size_bytes");
        if (!cJSON_IsNumber(size_item)) {
            size_item = cJSON_GetObjectItemCaseSensitive((cJSON *)source, "package_size");
        }
        if (!cJSON_IsNumber(size_item)) {
            size_item = cJSON_GetObjectItemCaseSensitive((cJSON *)source, "sz");
        }
        if (cJSON_IsNumber(size_item)) {
            size_bytes = (uint32_t)(size_item->valuedouble > 0 ? size_item->valuedouble : 0);
        }
    }
    out->package_size = size_bytes;

    {
        cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)source, "force_upgrade");
        if (cJSON_IsBool(item)) {
            out->force_upgrade = cJSON_IsTrue(item);
        }
        item = cJSON_GetObjectItemCaseSensitive((cJSON *)source, "allow_running_upgrade");
        if (cJSON_IsBool(item)) {
            out->allow_running_upgrade = cJSON_IsTrue(item);
        }
        item = cJSON_GetObjectItemCaseSensitive((cJSON *)source, "auto_commit");
        if (cJSON_IsBool(item)) {
            out->auto_commit = cJSON_IsTrue(item);
        }
    }

    if (out->upgrade_ticket[0] == '\0' ||
        out->release_code[0] == '\0' ||
        out->package_url[0] == '\0' ||
        out->package_sha256_hex[0] == '\0' ||
        out->package_etag[0] == '\0' ||
        out->package_size == 0U) {
        return -1;
    }
    return 0;
}

static int handle_upgrade_prepare_request(char *reply, size_t reply_cap,
                                          const char *corr, const char *session_ref,
                                          const char *scope, const char *action,
                                          const ota_prepare_payload_t *prepare_in)
{
    ota_upgrade_status_t status;
    ota_upgrade_status_t reject_status;
    ota_state_t duplicate_state = OTA_STATE_IDLE;
    const ota_upgrade_capability_t *cap = proto_ota_get_capability();
    ota_prepare_payload_t prepare;
    char extra[320];
    int result;

    if (reply == NULL || prepare_in == NULL) {
        return -1;
    }
    if (cap == NULL || !cap->ota_supported) {
        return build_action_nack(reply, reply_cap, corr, session_ref,
                                 scope, action, "controller",
                                 "MODULE_NOT_ENABLED", "ota transport not ready");
    }

    memset(&prepare, 0, sizeof(prepare));
    memcpy(&prepare, prepare_in, sizeof(prepare));
    result = proto_ota_execute_action(OTA_ACTION_PREPARE, &prepare);
    if (result == -5 && proto_ota_is_prepare_duplicate_accepted(&prepare, &duplicate_state)) {
        result = 1;
    }
    if (result == 0) {
        memset(&status, 0, sizeof(status));
        if (proto_ota_query_upgrade_status(&status) != 0 || status.ota_state != OTA_STATE_READY_TO_DOWNLOAD) {
            result = -1;
        }
    }
    if (result < 0) {
        memset(&reject_status, 0, sizeof(reject_status));
        if (proto_ota_query_upgrade_status(&reject_status) != 0) {
            memset(&reject_status, 0, sizeof(reject_status));
        }
        return build_action_nack(reply, reply_cap, corr, session_ref,
                                 scope, action, "controller",
                                 map_ota_reject_code(result, &reject_status),
                                 map_ota_reject_message(result, &reject_status));
    }

    if (result > 0) {
        (void)snprintf(extra, sizeof(extra),
                       "\"ut\":\"%s\",\"stg\":\"idempotent_replay\",\"ota_state\":\"%s\"",
                       prepare.upgrade_ticket,
                       map_ota_idempotent_stage(duplicate_state));
    } else {
        (void)snprintf(extra, sizeof(extra),
                       "\"ut\":\"%s\",\"stg\":\"accepted\"",
                       prepare.upgrade_ticket);
    }
    if (result == 0) {
        ota_start_request_after_ack();
    }
    return build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "controller", extra);
}

static const char *map_ota_reject_code(int result, const ota_upgrade_status_t *status)
{
    switch (result) {
    case -1:
        if (status != NULL && status->last_error_code != 0) {
            return "DEVICE_BUSY";
        }
        return "PARAM_INVALID";
    case -2:
        return "DEVICE_BUSY";
    case -3:
    case -4:
        return "MODULE_NOT_ENABLED";
    case -5:
        return "EXPIRED_COMMAND";
    default:
        return "CAPABILITY_NOT_EXPOSED";
    }
}

static const char *map_ota_reject_message(int result, const ota_upgrade_status_t *status)
{
    switch (result) {
    case -1:
        if (status != NULL && status->last_error_message[0] != '\0') {
            return status->last_error_message;
        }
        return "upgrade rejected";
    case -2:
        if (status != NULL && status->last_error_message[0] != '\0') {
            return status->last_error_message;
        }
        return "upgrade already in progress";
    case -3:
        if (status != NULL && status->last_error_message[0] != '\0') {
            return status->last_error_message;
        }
        return "upgrade port unavailable";
    case -4:
        if (status != NULL && status->last_error_message[0] != '\0') {
            return status->last_error_message;
        }
        return "rollback unsupported";
    case -5:
        return "duplicate upgrade token";
    default:
        if (status != NULL && status->last_error_message[0] != '\0') {
            return status->last_error_message;
        }
        return "upgrade rejected";
    }
}

static const char *map_ota_idempotent_stage(ota_state_t state)
{
    return proto_ota_stage_name(state, OTA_LAST_RESULT_NONE);
}

static uint8_t ota_state_blocks_remote_reboot(ota_state_t state)
{
    switch (state) {
    case OTA_STATE_PRECHECKING:
    case OTA_STATE_READY_TO_DOWNLOAD:
    case OTA_STATE_DOWNLOADING:
    case OTA_STATE_DOWNLOADED:
    case OTA_STATE_VERIFYING:
    case OTA_STATE_VERIFIED:
    case OTA_STATE_WRITING:
    case OTA_STATE_READY_TO_SWITCH:
    case OTA_STATE_SWITCHING:
    case OTA_STATE_ROLLING_BACK:
        return 1U;
    default:
        return 0U;
    }
}

static void remote_reboot_request_after_ack(void)
{
    s_remote_reboot_pending_ack = 1U;
    s_remote_reboot_armed = 0U;
    s_remote_reboot_due_ms = 0U;
}

static void ota_start_request_after_ack(void)
{
    s_ota_start_pending_ack = 1U;
    s_ota_start_armed = 0U;
    s_ota_start_due_ms = 0U;
}

void proto_execute_action_on_reply_send_result(int send_rc, uint32_t monotonic_ms)
{
    char line[128];

    if (s_remote_reboot_pending_ack != 0U) {
        s_remote_reboot_pending_ack = 0U;
        if (send_rc < 0) {
            s_remote_reboot_armed = 0U;
            s_remote_reboot_due_ms = 0U;
            bsp_debug_log("[PROTO] remote reboot cancelled: ACK send failed\r\n");
        } else {
            s_remote_reboot_armed = 1U;
            s_remote_reboot_due_ms = monotonic_ms + REMOTE_REBOOT_DELAY_MS;
            (void)snprintf(line, sizeof(line),
                           "[PROTO] remote reboot armed delay=%lu ms\r\n",
                           (unsigned long)REMOTE_REBOOT_DELAY_MS);
            bsp_debug_log(line);
        }
    }
    if (s_ota_start_pending_ack != 0U) {
        s_ota_start_pending_ack = 0U;
        if (send_rc < 0) {
            s_ota_start_armed = 0U;
            s_ota_start_due_ms = 0U;
            bsp_debug_log("[PROTO] OTA start cancelled: ACK send failed\r\n");
        } else {
            s_ota_start_armed = 1U;
            s_ota_start_due_ms = monotonic_ms + OTA_START_AFTER_ACK_MS;
            (void)snprintf(line, sizeof(line),
                           "[PROTO] OTA start armed delay=%lu ms\r\n",
                           (unsigned long)OTA_START_AFTER_ACK_MS);
            bsp_debug_log(line);
        }
    }
}

void proto_execute_action_poll(uint32_t monotonic_ms)
{
    const ota_port_t *ota_port;

    if (s_ota_start_armed != 0U) {
        if ((int32_t)(monotonic_ms - s_ota_start_due_ms) >= 0) {
            s_ota_start_armed = 0U;
            s_ota_start_due_ms = 0U;
            if (proto_ota_execute_action(OTA_ACTION_START, NULL) != 0) {
                bsp_debug_log("[PROTO] deferred OTA start failed\r\n");
            } else {
                bsp_debug_log("[PROTO] deferred OTA start launched\r\n");
            }
        }
    }

    if (s_remote_reboot_armed == 0U) {
        return;
    }
    if ((int32_t)(monotonic_ms - s_remote_reboot_due_ms) < 0) {
        return;
    }

    s_remote_reboot_armed = 0U;
    s_remote_reboot_due_ms = 0U;
    ota_port = ota_port_board();
    if (ota_port == NULL || ota_port->reboot_mcu == NULL) {
        bsp_debug_log("[PROTO] remote reboot skipped: reboot port unavailable\r\n");
        return;
    }
    if (ota_port->reboot_mcu(ota_port->user) != 0) {
        bsp_debug_log("[PROTO] remote reboot failed: reboot port returned\r\n");
    }
}

int proto_execute_action_handle(const char *json, size_t json_len, char *reply, size_t reply_cap)
{
    cJSON *root = NULL;
    cJSON *payload = NULL;
    cJSON *params = NULL;
    const device_config_t *cfg = NULL;
    const runtime_state_t *rs = NULL;
    char corr[48];
    char session_ref[64];
    char scope[16];
    char action[16];
    char target[32];
    char extra[320];
    char *params_json = NULL;
    int result;

    if (json == NULL || reply == NULL || reply_cap < 96U) {
        return -1;
    }

    corr[0] = '\0';
    session_ref[0] = '\0';
    scope[0] = '\0';
    action[0] = '\0';
    target[0] = '\0';
    extra[0] = '\0';

    root = cJSON_ParseWithLength(json, json_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        log_json_parse_failure("EX", json, json_len);
        return build_action_nack(reply, reply_cap, NULL, NULL, "", "", "", "PARAM_INVALID", "invalid json");
    }
    read_optional_string(root, "c", corr, sizeof(corr));
    read_optional_string(root, "r", session_ref, sizeof(session_ref));

    payload = cJSON_GetObjectItemCaseSensitive(root, "p");
    if (!cJSON_IsObject(payload)) {
        cJSON_Delete(root);
        return build_action_nack(reply, reply_cap, corr, session_ref, "", "", "", "PARAM_INVALID", "missing payload");
    }

    read_optional_string(payload, "sc", scope, sizeof(scope));
    read_optional_string(payload, "ac", action, sizeof(action));
    read_optional_string(payload, "tr", target, sizeof(target));
    params = cJSON_GetObjectItemCaseSensitive(payload, "pm");
    if (params == NULL) {
        params = cJSON_GetObjectItemCaseSensitive(payload, "params");
    }
    if (params != NULL && !cJSON_IsObject(params)) {
        cJSON_Delete(root);
        return build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "pm must be object");
    }
    if (scope[0] == '\0' || action[0] == '\0') {
        cJSON_Delete(root);
        return build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "missing sc or ac");
    }
    cfg = config_store_active();
    rs = runtime_state_get();
    if (params != NULL) {
        params_json = cJSON_PrintUnformatted(params);
    }

    if (strcmp(scope, "cm") == 0 && strcmp(action, "ppu") == 0) {
        char prompt_code[40];
        char prompt_json[96];

        prompt_code[0] = '\0';
        if (params != NULL) {
            read_optional_string(params, "pc", prompt_code, sizeof(prompt_code));
        }
        if (prompt_code[0] == '\0') {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", "PARAM_INVALID",
                                                           "missing pm.pc"));
        }
        (void)snprintf(prompt_json, sizeof(prompt_json), "{\"prompt_code\":\"%s\"}", prompt_code);
        result = safety_flow_execute_action("voice_broadcast", prompt_json, extra, sizeof(extra));
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", map_safety_reject_code(result), safety_flow_error_message(result)));
        }
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "controller", extra));
    }

    if (strcmp(scope, "cm") == 0 && strcmp(action, "rbt") == 0) {
        ota_upgrade_status_t status;
        const ota_port_t *ota_port = ota_port_board();

        if (require_target_or_empty(target, "controller") != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be controller"));
        }
        if (ota_port == NULL || ota_port->reboot_mcu == NULL) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", "MODULE_NOT_ENABLED",
                                                           "reboot port unavailable"));
        }
        memset(&status, 0, sizeof(status));
        if (proto_ota_query_upgrade_status(&status) != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", "DEVICE_BUSY",
                                                           "ota state unavailable"));
        }
        if (ota_state_blocks_remote_reboot(status.ota_state) != 0U) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", "DEVICE_BUSY",
                                                           "ota workflow active"));
        }
        if (rs != NULL &&
            (rs->run_state == RUNTIME_RUN_STARTING ||
             rs->run_state == RUNTIME_RUN_RUNNING ||
             rs->run_state == RUNTIME_RUN_STOPPING)) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", "DEVICE_BUSY",
                                                           "irrigation session active"));
        }
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        (void)snprintf(extra, sizeof(extra), "\"stg\":\"scheduled\"");
        result = build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "controller", extra);
        if (result >= 0) {
            remote_reboot_request_after_ack();
        }
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller", result);
    }

    if (strcmp(scope, "cm") == 0 && strcmp(action, "upg") == 0) {
        ota_prepare_payload_t prepare;
        const cJSON *upgrade_payload = params != NULL ? params : payload;

        if (parse_upgrade_prepare_payload(upgrade_payload, &prepare) != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "controller", "PARAM_INVALID",
                                                            "missing upgrade package fields"));
        }

        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "controller",
                                     handle_upgrade_prepare_request(reply, reply_cap, corr, session_ref,
                                                                    scope, action, &prepare));
    }

    if ((strcmp(scope, "wf") == 0 || strcmp(scope, "md") == 0) && strcmp(action, "pas") == 0) {
        if (strcmp(scope, "md") == 0 && require_target_or_empty(target, "pump_1") != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be pump_1"));
        }
        result = workflow_engine_request_pause_session();
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        if (result != WORKFLOW_REQ_OK) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, "pump_1", "DEVICE_BUSY",
                                                           workflow_control_error_message(result)));
        }
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "pump_1", NULL));
    }

    if ((strcmp(scope, "wf") == 0 || strcmp(scope, "md") == 0) && strcmp(action, "res") == 0) {
        if (strcmp(scope, "md") == 0 && require_target_or_empty(target, "pump_1") != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be pump_1"));
        }
        result = workflow_engine_request_resume_session();
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        if (result != WORKFLOW_REQ_OK) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, "pump_1", "DEVICE_BUSY",
                                                           workflow_control_error_message(result)));
        }
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "pump_1", NULL));
    }

    if (strcmp(scope, "md") == 0 && strcmp(action, "spu") == 0) {
        if (require_target(target, "pump_1") != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be pump_1"));
        }
        result = safety_flow_execute_action("start_pump", params_json, extra, sizeof(extra));
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "pump_1", map_safety_reject_code(result), safety_flow_error_message(result)));
        }
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "pump_1", extra));
    }

    if (strcmp(scope, "md") == 0 && strcmp(action, "tpu") == 0) {
        if (require_target(target, "pump_1") != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be pump_1"));
        }
        result = safety_flow_execute_action("stop_pump", params_json, extra, sizeof(extra));
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, "pump_1", map_safety_reject_code(result), safety_flow_error_message(result)));
        }
        return finalize_action_reply(reply, reply_cap, corr, scope, action, "pump_1",
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, "pump_1", extra));
    }

    if (strcmp(scope, "md") == 0 && strcmp(action, "ovl") == 0) {
        if (require_valve_target(target) != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be valve_1 or valve_2"));
        }
        if (cfg == NULL ||
            cfg->feature_modules.dual_valve_control == 0U) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "MODULE_NOT_ENABLED", "valve module disabled"));
        }
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        result = execute_bound_valve_action(cfg, target, "open_valve");
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, target,
                                                           result == -2 ? "MODULE_NOT_ENABLED" : "DEVICE_BUSY",
                                                           "valve open failed"));
        }
        (void)snprintf(extra, sizeof(extra), "\"st\":\"open\"");
        return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, target, extra));
    }

    if (strcmp(scope, "md") == 0 && strcmp(action, "cvl") == 0) {
        if (require_valve_target(target) != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be valve_1 or valve_2"));
        }
        if (cfg == NULL ||
            cfg->feature_modules.dual_valve_control == 0U) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "MODULE_NOT_ENABLED", "valve module disabled"));
        }
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        result = execute_bound_valve_action(cfg, target, "close_valve");
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, target,
                                                           result == -2 ? "MODULE_NOT_ENABLED" : "DEVICE_BUSY",
                                                           "valve close failed"));
        }
        (void)snprintf(extra, sizeof(extra), "\"st\":\"closed\"");
        return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, target, extra));
    }

    if (strcmp(scope, "md") == 0 && strcmp(action, "orl") == 0) {
        if (require_relay_target(target) != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be relay_1 or relay_2"));
        }
        if (cfg == NULL || cfg->feature_modules.relay_output_control == 0U) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "MODULE_NOT_ENABLED", "relay module disabled"));
        }
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        result = execute_bound_relay_action(cfg, target, "open_relay");
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, target,
                                                           result == -2 ? "MODULE_NOT_ENABLED" : "DEVICE_BUSY",
                                                           "relay open failed"));
        }
        (void)snprintf(extra, sizeof(extra), "\"st\":\"on\"");
        return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, target, extra));
    }

    if (strcmp(scope, "md") == 0 && strcmp(action, "crl") == 0) {
        if (require_relay_target(target) != 0) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "PARAM_INVALID", "tr must be relay_1 or relay_2"));
        }
        if (cfg == NULL || cfg->feature_modules.relay_output_control == 0U) {
            if (params_json != NULL) {
                cJSON_free(params_json);
            }
            cJSON_Delete(root);
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "MODULE_NOT_ENABLED", "relay module disabled"));
        }
        if (params_json != NULL) {
            cJSON_free(params_json);
        }
        cJSON_Delete(root);
        result = execute_bound_relay_action(cfg, target, "close_relay");
        if (result != 0) {
            return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                         build_action_nack(reply, reply_cap, corr, session_ref,
                                                           scope, action, target,
                                                           result == -2 ? "MODULE_NOT_ENABLED" : "DEVICE_BUSY",
                                                           "relay close failed"));
        }
        (void)snprintf(extra, sizeof(extra), "\"st\":\"off\"");
        return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                     build_action_ack(reply, reply_cap, corr, session_ref, scope, action, target, extra));
    }

    if (params_json != NULL) {
        cJSON_free(params_json);
    }
    cJSON_Delete(root);
    return finalize_action_reply(reply, reply_cap, corr, scope, action, target,
                                 build_action_nack(reply, reply_cap, corr, session_ref, scope, action, target, "UNSUPPORTED_COMMAND", "unsupported action"));
}
