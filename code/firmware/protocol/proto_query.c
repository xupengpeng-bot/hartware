#include "proto_query.h"

#include "common_status.h"
#include "config_store.h"
#include "module_meter.h"
#include "proto_codec_json.h"
#include "proto_command.h"
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

static int build_query_nack(char *reply, size_t reply_cap, const char *corr, const char *session_ref,
                            const char *reject_code, const char *reason)
{
    return proto_build_command_nack(reply, reply_cap,
                                    corr != NULL && corr[0] != '\0' ? corr : NULL,
                                    session_ref != NULL && session_ref[0] != '\0' ? session_ref : NULL,
                                    corr != NULL && corr[0] != '\0' ? corr : "query",
                                    "QUERY",
                                    reject_code,
                                    reason,
                                    NULL);
}

int proto_query_handle(const char *json, size_t json_len, char *reply, size_t reply_cap)
{
    cJSON *root = NULL;
    cJSON *payload = NULL;
    char corr[48];
    char session_ref[64];
    char scope[16];
    char qcode[16];
    const runtime_state_t *rs;
    const common_status_t *cs;
    int rc;

    (void)json_len;

    if (json == NULL || reply == NULL || reply_cap < 96U) {
        return -1;
    }

    corr[0] = '\0';
    session_ref[0] = '\0';
    scope[0] = '\0';
    qcode[0] = '\0';

    root = cJSON_ParseWithLength(json, json_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        log_json_parse_failure("QR", json, json_len);
        return build_query_nack(reply, reply_cap, NULL, NULL, "PARAM_INVALID", "invalid json");
    }
    read_optional_string(root, "c", corr, sizeof(corr));
    read_optional_string(root, "r", session_ref, sizeof(session_ref));

    payload = cJSON_GetObjectItemCaseSensitive(root, "p");
    if (!cJSON_IsObject(payload)) {
        cJSON_Delete(root);
        return build_query_nack(reply, reply_cap, corr, session_ref, "PARAM_INVALID", "bad query payload");
    }

    read_optional_string_alias(payload, "sc", "scope", scope, sizeof(scope));
    read_optional_string_alias(payload, "qc", "query_code", qcode, sizeof(qcode));
    if (scope[0] == '\0' || qcode[0] == '\0') {
        cJSON_Delete(root);
        return build_query_nack(reply, reply_cap, corr, session_ref, "PARAM_INVALID", "missing sc or qc");
    }

    rs = runtime_state_get();
    cs = common_status_get();
    if (rs == NULL || cs == NULL) {
        cJSON_Delete(root);
        return build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "runtime not ready");
    }

    if (strcmp(scope, "cm") == 0 && strcmp(qcode, "qcs") == 0) {
        cJSON *out_payload = cJSON_CreateObject();
        uint8_t signal_valid = (rs->signal_csq >= 0 && rs->signal_csq <= 99) ? 1U : 0U;
        uint8_t battery_v_valid = (cs->battery_voltage_v >= 0.1f && cs->battery_voltage_v <= 64.0f) ? 1U : 0U;
        uint8_t solar_v_valid = (cs->solar_voltage_v >= 0.0f && cs->solar_voltage_v <= 64.0f) ? 1U : 0U;
        uint8_t battery_soc_valid = (battery_v_valid != 0U && rs->battery_soc <= 100U) ? 1U : 0U;

        if (out_payload == NULL ||
            cJSON_AddNumberToObject(out_payload, "rd", rs->ready ? 1.0 : 0.0) == NULL ||
            cJSON_AddNumberToObject(out_payload, "on", rs->online ? 1.0 : 0.0) == NULL ||
            cJSON_AddNumberToObject(out_payload, "tc", rs->tcp_connected ? 1.0 : 0.0) == NULL ||
            cJSON_AddStringToObject(out_payload, "wf",
                                    proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                          rs->ready ? 1U : 0U)) == NULL ||
            cJSON_AddNumberToObject(out_payload, "rt", (double)rs->runtime_sec) == NULL ||
            cJSON_AddNumberToObject(out_payload, "me", (double)rs->meter_epoch) == NULL ||
            add_number_if_valid(out_payload, "csq", (double)rs->signal_csq, signal_valid) != 0 ||
            add_number_if_valid(out_payload, "bs", (double)rs->battery_soc, battery_soc_valid) != 0 ||
            add_number_if_valid(out_payload, "bv", (double)cs->battery_voltage_v, battery_v_valid) != 0 ||
            add_number_if_valid(out_payload, "sv", (double)cs->solar_voltage_v, solar_v_valid) != 0 ||
            cJSON_AddNumberToObject(out_payload, "cv", (double)rs->config_version) == NULL ||
            cJSON_AddStringToObject(out_payload, "pm",
                                    proto_map_power_mode_short(runtime_state_power_name(rs->power_state))) == NULL ||
            add_number_if_valid(out_payload, "fq", (double)rs->total_m3,
                                (uint8_t)(rs->total_m3 >= 0.0f && rs->total_m3 <= 10000000.0f)) != 0 ||
            add_number_if_valid(out_payload, "ek", (double)rs->energy_kwh,
                                (uint8_t)(rs->energy_kwh >= 0.0f && rs->energy_kwh <= 10000000.0f)) != 0 ||
            (breaker_state_value(rs) != NULL &&
             add_string_if_nonempty(out_payload, "brs", breaker_state_value(rs)) != 0)) {
            cJSON_Delete(root);
            cJSON_Delete(out_payload);
            return build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "build qcs failed");
        }
        rc = proto_json_build_message(reply, reply_cap, PROTO_MSG_QUERY_RESULT, 0U,
                                      corr[0] != '\0' ? corr : NULL,
                                      session_ref[0] != '\0' ? session_ref : NULL,
                                      out_payload);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "send qcs failed");
    }

    if (strcmp(scope, "wf") == 0 && strcmp(qcode, "qwf") == 0) {
        cJSON *out_payload = cJSON_CreateObject();

        if (out_payload == NULL ||
            cJSON_AddStringToObject(out_payload, "wf",
                                    proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                          rs->ready ? 1U : 0U)) == NULL) {
            cJSON_Delete(root);
            cJSON_Delete(out_payload);
            return build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "build qwf failed");
        }
        rc = proto_json_build_message(reply, reply_cap, PROTO_MSG_QUERY_RESULT, 0U,
                                      corr[0] != '\0' ? corr : NULL,
                                      session_ref[0] != '\0' ? session_ref : NULL,
                                      out_payload);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "send qwf failed");
    }

    if (strcmp(scope, "cm") == 0 && strcmp(qcode, "qem") == 0) {
        cJSON *out_payload = cJSON_CreateObject();
        uint8_t meter_valid = (rs->meter_last.valid != 0U) ? 1U : 0U;

        if (out_payload == NULL ||
            meter_valid == 0U ||
            (meter_protocol_value() != NULL &&
             add_string_if_nonempty(out_payload, "mp", meter_protocol_value()) != 0) ||
            cJSON_AddNumberToObject(out_payload, "me", (double)rs->meter_epoch) == NULL ||
            add_number_if_valid(out_payload, "vv", (double)rs->voltage_v,
                                (uint8_t)(rs->voltage_v >= 0.0f && rs->voltage_v <= 1000.0f)) != 0 ||
            add_number_if_valid(out_payload, "ia", (double)rs->current_a,
                                (uint8_t)(rs->current_a >= 0.0f && rs->current_a <= 1000.0f)) != 0 ||
            add_number_if_valid(out_payload, "pw", (double)rs->power_kw,
                                (uint8_t)(rs->power_kw >= 0.0f && rs->power_kw <= 500.0f)) != 0 ||
            add_number_if_valid(out_payload, "ek", (double)rs->energy_kwh,
                                (uint8_t)(rs->energy_kwh >= 0.0f && rs->energy_kwh <= 10000000.0f)) != 0) {
            cJSON_Delete(root);
            cJSON_Delete(out_payload);
            return build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "build qem failed");
        }
        rc = proto_json_build_message(reply, reply_cap, PROTO_MSG_QUERY_RESULT, 0U,
                                      corr[0] != '\0' ? corr : NULL,
                                      session_ref[0] != '\0' ? session_ref : NULL,
                                      out_payload);
        cJSON_Delete(root);
        return rc >= 0 ? rc : build_query_nack(reply, reply_cap, corr, session_ref, "DEVICE_BUSY", "send qem failed");
    }

    cJSON_Delete(root);
    return build_query_nack(reply, reply_cap, corr, session_ref, "UNSUPPORTED_COMMAND", "unsupported query");
}
