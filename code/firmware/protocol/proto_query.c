#include "proto_query.h"

#include "common_status.h"
#include "config_store.h"
#include "module_meter.h"
#include "proto_codec_json.h"
#include "proto_command.h"
#include "proto_ota.h"
#include "proto_envelope.h"
#include "proto_json_builder.h"
#include "runtime_state.h"

#include "bsp_uart.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>

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

static void read_optional_string_alias(const cJSON *obj, const char *primary_key, const char *alias_key,
                                       char *out, size_t out_cap)
{
    read_optional_string(obj, primary_key, out, out_cap);
    if (out != NULL && out_cap > 0U && out[0] == '\0' && alias_key != NULL) {
        read_optional_string(obj, alias_key, out, out_cap);
    }
}

static int add_number_if_valid(cJSON *obj, const char *key, double value, uint8_t valid)
{
    if (obj == NULL || key == NULL) {
        return -1;
    }
    if (valid == 0U) {
        return 0;
    }
    return cJSON_AddNumberToObject(obj, key, value) != NULL ? 0 : -1;
}

static int add_string_if_nonempty(cJSON *obj, const char *key, const char *value)
{
    if (obj == NULL || key == NULL || value == NULL || value[0] == '\0') {
        return -1;
    }
    return cJSON_AddStringToObject(obj, key, value) != NULL ? 0 : -1;
}

static int add_bool_number(cJSON *obj, const char *key, bool value)
{
    if (obj == NULL || key == NULL) {
        return -1;
    }
    return cJSON_AddNumberToObject(obj, key, value ? 1.0 : 0.0) != NULL ? 0 : -1;
}

static int scope_is_common(const char *scope)
{
    return scope != NULL && (strcmp(scope, "cm") == 0 || strcmp(scope, "common") == 0);
}

static int query_code_matches(const char *qcode, const char *short_code, const char *long_code)
{
    if (qcode == NULL) {
        return 0;
    }
    if (short_code != NULL && strcmp(qcode, short_code) == 0) {
        return 1;
    }
    if (long_code != NULL && strcmp(qcode, long_code) == 0) {
        return 1;
    }
    return 0;
}

static const char *resolve_reply_session_ref(const char *explicit_session_ref)
{
    const runtime_state_t *rs;

    if (explicit_session_ref != NULL && explicit_session_ref[0] != '\0') {
        return explicit_session_ref;
    }
    rs = runtime_state_get();
    if (rs != NULL && rs->session_ref[0] != '\0') {
        return rs->session_ref;
    }
    return NULL;
}

static const char *breaker_state_value(const runtime_state_t *rs)
{
    const device_config_t *cfg = config_store_active();

    if (cfg == NULL || cfg->feature_modules.breaker_feedback_monitor == 0U) {
        return NULL;
    }
    if (rs == NULL) {
        return NULL;
    }
    return rs->pump_state == RUNTIME_PUMP_RUNNING ? "closed" : "opened";
}

static const char *meter_protocol_value(void)
{
    const device_config_t *cfg = config_store_active();

    if (cfg == NULL || cfg->feature_modules.electric_meter_modbus == 0U) {
        return NULL;
    }
    return module_meter_source_name();
}

static void log_query_result_failure(const char *qcode, const char *stage, int rc, size_t reply_cap)
{
    char line[192];

    (void)snprintf(line, sizeof(line),
                   "[PROTO] QS build failed qc=%s stage=%s rc=%ld cap=%lu\r\n",
                   qcode != NULL ? qcode : "",
                   stage != NULL ? stage : "unknown",
                   (long)rc,
                   (unsigned long)reply_cap);
    bsp_debug_log(line);
}

static int append_query_sep(json_buf_t *jb, uint8_t *first)
{
    if (jb == NULL || first == NULL) {
        return -1;
    }
    if (*first != 0U) {
        *first = 0U;
        return 0;
    }
    return json_buf_append(jb, ",");
}

static int append_query_string_field(json_buf_t *jb, uint8_t *first, const char *key, const char *value)
{
    if (jb == NULL || first == NULL || key == NULL || value == NULL) {
        return -1;
    }
    if (append_query_sep(jb, first) != 0 ||
        json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":\"") != 0 ||
        json_escape_append(jb, value) != 0 ||
        json_buf_append(jb, "\"") != 0) {
        return -1;
    }
    return 0;
}

static int append_query_u32_field(json_buf_t *jb, uint8_t *first, const char *key, uint32_t value)
{
    if (jb == NULL || first == NULL || key == NULL) {
        return -1;
    }
    if (append_query_sep(jb, first) != 0 ||
        json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append_fmt(jb, "\":%lu", (unsigned long)value) != 0) {
        return -1;
    }
    return 0;
}

static int append_query_i32_field(json_buf_t *jb, uint8_t *first, const char *key, int32_t value)
{
    if (jb == NULL || first == NULL || key == NULL) {
        return -1;
    }
    if (append_query_sep(jb, first) != 0 ||
        json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append_fmt(jb, "\":%ld", (long)value) != 0) {
        return -1;
    }
    return 0;
}

static int append_query_fixed_field(json_buf_t *jb, uint8_t *first, const char *key, float value, uint8_t frac_digits)
{
    if (jb == NULL || first == NULL || key == NULL) {
        return -1;
    }
    if (append_query_sep(jb, first) != 0 ||
        json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0 ||
        json_buf_append_fixed(jb, value, frac_digits) != 0) {
        return -1;
    }
    return 0;
}

static int append_query_string_if_nonempty(json_buf_t *jb, uint8_t *first, const char *key, const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    return append_query_string_field(jb, first, key, value);
}

static int build_query_result_workflow_state(char *reply, size_t reply_cap, const char *corr,
                                             const char *session_ref, const char *scope, const char *qcode,
                                             const runtime_state_t *rs)
{
    json_buf_t jb;
    uint8_t first = 1U;
    int rc;

    json_buf_init(&jb, reply, reply_cap);
    rc = proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY_RESULT, 0U,
                                              corr[0] != '\0' ? corr : NULL,
                                              resolve_reply_session_ref(session_ref));
    if (rc != 0) {
        log_query_result_failure(qcode, "prefix", rc, reply_cap);
        return -101;
    }
    if (append_query_string_field(&jb, &first, "sc", scope) != 0 ||
        append_query_string_field(&jb, &first, "qc", qcode) != 0 ||
        append_query_string_field(&jb, &first, "wf",
                                  proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                        rs->ready ? 1U : 0U)) != 0) {
        log_query_result_failure(qcode, "payload", -1, reply_cap);
        return -102;
    }
    rc = proto_envelope_close_payload(&jb);
    if (rc != 0) {
        log_query_result_failure(qcode, "close", rc, reply_cap);
        return -103;
    }
    return (int)jb.len;
}

static int build_query_result_common_status(char *reply, size_t reply_cap, const char *corr,
                                            const char *session_ref, const char *scope, const char *qcode,
                                            const runtime_state_t *rs, const common_status_t *cs)
{
    json_buf_t jb;
    uint8_t first = 1U;
    uint8_t signal_valid = (uint8_t)(rs->signal_csq >= 0 && rs->signal_csq <= 99);
    uint8_t battery_v_valid = (uint8_t)(cs->battery_voltage_v >= 0.1f && cs->battery_voltage_v <= 64.0f);
    uint8_t solar_v_valid = (uint8_t)(cs->solar_voltage_v >= 0.0f && cs->solar_voltage_v <= 64.0f);
    uint8_t battery_soc_valid = (uint8_t)(battery_v_valid != 0U && rs->battery_soc <= 100U);
    const char *breaker_state = breaker_state_value(rs);
    int rc;

    json_buf_init(&jb, reply, reply_cap);
    rc = proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY_RESULT, 0U,
                                              corr[0] != '\0' ? corr : NULL,
                                              resolve_reply_session_ref(session_ref));
    if (rc != 0) {
        log_query_result_failure(qcode, "prefix", rc, reply_cap);
        return -101;
    }
    if (append_query_string_field(&jb, &first, "sc", scope) != 0 ||
        append_query_string_field(&jb, &first, "qc", qcode) != 0 ||
        append_query_u32_field(&jb, &first, "rd", rs->ready ? 1U : 0U) != 0 ||
        append_query_u32_field(&jb, &first, "on", rs->online ? 1U : 0U) != 0 ||
        append_query_u32_field(&jb, &first, "tc", rs->tcp_connected ? 1U : 0U) != 0 ||
        append_query_string_field(&jb, &first, "wf",
                                  proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                        rs->ready ? 1U : 0U)) != 0 ||
        append_query_u32_field(&jb, &first, "rt", rs->runtime_sec) != 0 ||
        append_query_u32_field(&jb, &first, "me", rs->meter_epoch) != 0 ||
        append_query_u32_field(&jb, &first, "cv", rs->config_version) != 0 ||
        append_query_string_field(&jb, &first, "pm",
                                  proto_map_power_mode_short(runtime_state_power_name(rs->power_state))) != 0 ||
        append_query_fixed_field(&jb, &first, "fq", rs->total_m3, 2U) != 0 ||
        append_query_fixed_field(&jb, &first, "ek", rs->energy_kwh, 2U) != 0) {
        log_query_result_failure(qcode, "payload", -1, reply_cap);
        return -102;
    }
    if ((signal_valid != 0U && append_query_i32_field(&jb, &first, "csq", rs->signal_csq) != 0) ||
        (battery_soc_valid != 0U && append_query_u32_field(&jb, &first, "bs", rs->battery_soc) != 0) ||
        (battery_v_valid != 0U && append_query_fixed_field(&jb, &first, "bv", cs->battery_voltage_v, 2U) != 0) ||
        (solar_v_valid != 0U && append_query_fixed_field(&jb, &first, "sv", cs->solar_voltage_v, 2U) != 0) ||
        append_query_string_if_nonempty(&jb, &first, "brs", breaker_state) != 0) {
        log_query_result_failure(qcode, "optional", -1, reply_cap);
        return -103;
    }
    rc = proto_envelope_close_payload(&jb);
    if (rc != 0) {
        log_query_result_failure(qcode, "close", rc, reply_cap);
        return -104;
    }
    return (int)jb.len;
}

static int build_query_result_electric_meter(char *reply, size_t reply_cap, const char *corr,
                                             const char *session_ref, const char *scope, const char *qcode,
                                             const runtime_state_t *rs)
{
    json_buf_t jb;
    uint8_t first = 1U;
    const char *meter_protocol = meter_protocol_value();
    int rc;

    json_buf_init(&jb, reply, reply_cap);
    rc = proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY_RESULT, 0U,
                                              corr[0] != '\0' ? corr : NULL,
                                              resolve_reply_session_ref(session_ref));
    if (rc != 0) {
        log_query_result_failure(qcode, "prefix", rc, reply_cap);
        return -101;
    }
    if (append_query_string_field(&jb, &first, "sc", scope) != 0 ||
        append_query_string_field(&jb, &first, "qc", qcode) != 0 ||
        append_query_string_field(&jb, &first, "wf",
                                  proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                        rs->ready ? 1U : 0U)) != 0 ||
        append_query_u32_field(&jb, &first, "me", rs->meter_epoch) != 0 ||
        append_query_u32_field(&jb, &first, "rt", rs->runtime_sec) != 0 ||
        append_query_fixed_field(&jb, &first, "vv", rs->voltage_v, 1U) != 0 ||
        append_query_fixed_field(&jb, &first, "ia", rs->current_a, 1U) != 0 ||
        append_query_fixed_field(&jb, &first, "pw", rs->power_kw, 2U) != 0 ||
        append_query_fixed_field(&jb, &first, "fq", rs->total_m3, 2U) != 0 ||
        append_query_fixed_field(&jb, &first, "ek", rs->energy_kwh, 2U) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "mp", meter_protocol) != 0) {
        log_query_result_failure(qcode, "payload", -1, reply_cap);
        return -102;
    }
    rc = proto_envelope_close_payload(&jb);
    if (rc != 0) {
        log_query_result_failure(qcode, "close", rc, reply_cap);
        return -103;
    }
    return (int)jb.len;
}

static int build_query_result_upgrade_status(char *reply, size_t reply_cap, const char *corr,
                                             const char *session_ref, const char *scope, const char *qcode,
                                             const ota_upgrade_status_t *status)
{
    json_buf_t jb;
    uint8_t first = 1U;
    int rc;

    json_buf_init(&jb, reply, reply_cap);
    rc = proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY_RESULT, 0U,
                                              corr[0] != '\0' ? corr : NULL,
                                              resolve_reply_session_ref(session_ref));
    if (rc != 0) {
        log_query_result_failure(qcode, "prefix", rc, reply_cap);
        return -101;
    }
    if (append_query_string_field(&jb, &first, "sc", scope) != 0 ||
        append_query_string_field(&jb, &first, "qc", qcode) != 0 ||
        append_query_string_field(&jb, &first, "ota_stage", status->ota_stage) != 0 ||
        append_query_u32_field(&jb, &first, "ota_state", status->ota_state) != 0 ||
        append_query_string_field(&jb, &first, "current_version", status->current_version) != 0 ||
        append_query_u32_field(&jb, &first, "last_result", status->last_result) != 0 ||
        append_query_u32_field(&jb, &first, "last_error_code", status->last_error_code) != 0 ||
        append_query_u32_field(&jb, &first, "download_progress_pct", status->download_progress_pct) != 0 ||
        append_query_u32_field(&jb, &first, "write_progress_pct", status->write_progress_pct) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "target_version", status->target_version) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "package_sha256_hex", status->package_sha256_hex) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "package_etag", status->package_etag) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "last_error_message", status->last_error_message) != 0) {
        log_query_result_failure(qcode, "payload", -1, reply_cap);
        return -102;
    }
    rc = proto_envelope_close_payload(&jb);
    if (rc != 0) {
        log_query_result_failure(qcode, "close", rc, reply_cap);
        return -103;
    }
    return (int)jb.len;
}

static int build_query_result_upgrade_capability(char *reply, size_t reply_cap, const char *corr,
                                                 const char *session_ref, const char *scope, const char *qcode,
                                                 const ota_upgrade_capability_t *cap)
{
    json_buf_t jb;
    uint8_t first = 1U;
    int rc;

    json_buf_init(&jb, reply, reply_cap);
    rc = proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY_RESULT, 0U,
                                              corr[0] != '\0' ? corr : NULL,
                                              resolve_reply_session_ref(session_ref));
    if (rc != 0) {
        log_query_result_failure(qcode, "prefix", rc, reply_cap);
        return -101;
    }
    if (append_query_string_field(&jb, &first, "sc", scope) != 0 ||
        append_query_string_field(&jb, &first, "qc", qcode) != 0 ||
        append_query_u32_field(&jb, &first, "ota_supported", cap->ota_supported ? 1U : 0U) != 0 ||
        append_query_u32_field(&jb, &first, "dual_bank", cap->dual_bank ? 1U : 0U) != 0 ||
        append_query_u32_field(&jb, &first, "min_battery_soc_default", cap->min_battery_soc_default) != 0 ||
        append_query_u32_field(&jb, &first, "min_signal_csq_default", cap->min_signal_csq_default) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "package_formats", cap->package_formats) != 0 ||
        append_query_string_if_nonempty(&jb, &first, "compression_formats", cap->compression_formats) != 0) {
        log_query_result_failure(qcode, "payload", -1, reply_cap);
        return -102;
    }
    rc = proto_envelope_close_payload(&jb);
    if (rc != 0) {
        log_query_result_failure(qcode, "close", rc, reply_cap);
        return -103;
    }
    return (int)jb.len;
}

static int build_query_nack(char *reply, size_t reply_cap, const char *corr, const char *session_ref,
                            const char *scope, const char *qcode,
                            const char *reject_code, const char *reason)
{
    char extra[128];
    const runtime_state_t *rs = runtime_state_get();
    int wrote;

    wrote = snprintf(extra, sizeof(extra),
                     "\"sc\":\"%s\",\"qc\":\"%s\",\"wf\":\"%s\"",
                     scope != NULL ? scope : "",
                     qcode != NULL ? qcode : "",
                     proto_map_workflow_short_from_runtime(
                         runtime_state_workflow_name(rs != NULL ? rs->workflow_state : RUNTIME_WORKFLOW_NOT_READY),
                         (rs != NULL && rs->ready) ? 1U : 0U));
    if (wrote < 0 || (size_t)wrote >= sizeof(extra)) {
        return -1;
    }
    return proto_build_command_nack(reply, reply_cap,
                                    corr != NULL && corr[0] != '\0' ? corr : NULL,
                                    resolve_reply_session_ref(session_ref),
                                    corr != NULL && corr[0] != '\0' ? corr : "query",
                                    "QUERY",
                                    reject_code,
                                    reason,
                                    extra);
}

int proto_query_handle(const char *json, size_t json_len, char *reply, size_t reply_cap)
{
    cJSON *root = NULL;
    cJSON *payload = NULL;
    char repaired_json[2049];
    char corr[48];
    char session_ref[64];
    char scope[16];
    char qcode[16];
    const runtime_state_t *rs;
    const common_status_t *cs;
    int rc;
    size_t repaired_len = 0U;

    (void)json_len;

    if (json == NULL || reply == NULL || reply_cap < 96U) {
        return -1;
    }

    corr[0] = '\0';
    session_ref[0] = '\0';
    scope[0] = '\0';
    qcode[0] = '\0';

    root = cJSON_ParseWithLength(json, json_len);
    if ((root == NULL || !cJSON_IsObject(root)) && json_len + 1U <= sizeof(repaired_json)) {
        if (root != NULL) {
            cJSON_Delete(root);
            root = NULL;
        }
        repaired_len = proto_json_repair_duplicate_separators(json, json_len,
                                                              repaired_json, sizeof(repaired_json));
        if (repaired_len > 0U) {
            root = cJSON_ParseWithLength(repaired_json, repaired_len);
            if (root != NULL && cJSON_IsObject(root)) {
                bsp_debug_log("[PROTO] QR repaired duplicate separators in inbound json\r\n");
            }
        }
    }
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        log_json_parse_failure("QR", json, json_len);
        return build_query_nack(reply, reply_cap, NULL, NULL, "", "", "PARAM_INVALID", "invalid json");
    }
    read_optional_string(root, "c", corr, sizeof(corr));
    read_optional_string(root, "r", session_ref, sizeof(session_ref));

    payload = cJSON_GetObjectItemCaseSensitive(root, "p");
    if (!cJSON_IsObject(payload)) {
        cJSON_Delete(root);
        return build_query_nack(reply, reply_cap, corr, session_ref, "", "", "PARAM_INVALID", "bad query payload");
    }

    read_optional_string_alias(payload, "sc", "scope", scope, sizeof(scope));
    read_optional_string_alias(payload, "qc", "query_code", qcode, sizeof(qcode));
    if (scope[0] == '\0' || qcode[0] == '\0') {
        cJSON_Delete(root);
        return build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "PARAM_INVALID", "missing sc or qc");
    }

    rs = runtime_state_get();
    cs = common_status_get();
    if (rs == NULL || cs == NULL) {
        cJSON_Delete(root);
        return build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "runtime not ready");
    }

    if (scope_is_common(scope) && query_code_matches(qcode, "qcs", NULL)) {
        rc = build_query_result_common_status(reply, reply_cap, corr, session_ref, scope, qcode, rs, cs);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "send qcs failed");
    }

    if (strcmp(scope, "wf") == 0 && strcmp(qcode, "qwf") == 0) {
        rc = build_query_result_workflow_state(reply, reply_cap, corr, session_ref, scope, qcode, rs);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "send qwf failed");
    }

    if (scope_is_common(scope) && query_code_matches(qcode, "qem", NULL)) {
        rc = build_query_result_electric_meter(reply, reply_cap, corr, session_ref, scope, qcode, rs);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "send qem failed");
    }

    if (scope_is_common(scope) && query_code_matches(qcode, "qgs", NULL)) {
        ota_upgrade_status_t status;

        if (proto_ota_query_upgrade_status(&status) != 0) {
            cJSON_Delete(root);
            return build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "build upgrade status failed");
        }
        rc = build_query_result_upgrade_status(reply, reply_cap, corr, session_ref, scope, qcode, &status);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "send upgrade status failed");
    }

    if (scope_is_common(scope) && query_code_matches(qcode, "qgc", NULL)) {
        ota_upgrade_capability_t cap;

        if (proto_ota_query_upgrade_capability(&cap) != 0) {
            cJSON_Delete(root);
            return build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "build upgrade capability failed");
        }
        rc = build_query_result_upgrade_capability(reply, reply_cap, corr, session_ref, scope, qcode, &cap);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "DEVICE_BUSY", "send upgrade capability failed");
    }

    cJSON_Delete(root);
    return build_query_nack(reply, reply_cap, corr, session_ref, scope, qcode, "UNSUPPORTED_COMMAND", "unsupported query");
}
