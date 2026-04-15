#include "proto_event_report.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "net_connectivity.h"

#include <stdio.h>
#include <stdint.h>

static int append_optional_string_field(json_buf_t *jb, const char *key, const char *value)
{
    if (jb == NULL || key == NULL || value == NULL || value[0] == '\0') {
        return -1;
    }
    if (json_buf_append(jb, ",\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":\"") != 0 ||
        json_escape_append(jb, value) != 0 ||
        json_buf_append(jb, "\"") != 0) {
        return -1;
    }
    return 0;
}

static int append_u32_field(json_buf_t *jb, const char *key, uint32_t value)
{
    if (jb == NULL || key == NULL) {
        return -1;
    }
    if (json_buf_append(jb, ",\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0 ||
        json_buf_append_fmt(jb, "%lu", (unsigned long)value) != 0) {
        return -1;
    }
    return 0;
}

static int append_fixed_field(json_buf_t *jb, const char *key, float value, uint8_t frac_digits)
{
    if (jb == NULL || key == NULL) {
        return -1;
    }
    if (json_buf_append(jb, ",\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0 ||
        json_buf_append_fixed(jb, value, frac_digits) != 0) {
        return -1;
    }
    return 0;
}

int proto_event_report_send_min(const char *session_ref,
                                const char *event_code,
                                const char *reject_code,
                                const char *message,
                                const char *target_ref)
{
    json_buf_t jb;
    char       wire_json[320];

    if (event_code == NULL || event_code[0] == '\0') {
        return -1;
    }

    json_buf_init(&jb, wire_json, sizeof(wire_json));
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_EVENT_REPORT, 0U, NULL, session_ref) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"ec\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, event_code) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"") != 0) {
        return -2;
    }
    if ((reject_code != NULL && reject_code[0] != '\0' &&
         append_optional_string_field(&jb, "rc", reject_code) != 0) ||
        (message != NULL && message[0] != '\0' &&
         append_optional_string_field(&jb, "msg", message) != 0) ||
        (target_ref != NULL && target_ref[0] != '\0' &&
         append_optional_string_field(&jb, "tr", target_ref) != 0)) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return net_connectivity_send_json(wire_json, jb.len);
}

int proto_event_report_send_counter_reset(const char *session_ref,
                                          const char *reason_code,
                                          uint32_t meter_epoch,
                                          uint32_t runtime_sec,
                                          float total_m3,
                                          float energy_kwh)
{
    json_buf_t jb;
    char wire_json[384];

    json_buf_init(&jb, wire_json, sizeof(wire_json));
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_EVENT_REPORT, 0U, NULL, session_ref) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"ec\":\"crs\"") != 0 ||
        (reason_code != NULL && reason_code[0] != '\0' &&
         append_optional_string_field(&jb, "rc", reason_code) != 0) ||
        append_u32_field(&jb, "me", meter_epoch) != 0 ||
        append_u32_field(&jb, "rt", runtime_sec) != 0 ||
        append_fixed_field(&jb, "fq", total_m3, 2U) != 0 ||
        append_fixed_field(&jb, "ek", energy_kwh, 2U) != 0) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return net_connectivity_send_json(wire_json, jb.len);
}

int proto_event_report_send_upgrade_report(const char *upgrade_token,
                                           const char *upgrade_job_id,
                                           const char *upgrade_item_id,
                                           const char *release_id,
                                           const char *release_code,
                                           const char *package_artifact_id,
                                           const char *stage,
                                           const char *result,
                                           uint8_t progress_percent,
                                           const char *reason_code,
                                           const char *message,
                                           const char *firmware_version,
                                           const char *checksum)
{
    json_buf_t jb;
    char wire_json[768];

    if (upgrade_token == NULL || upgrade_token[0] == '\0' ||
        stage == NULL || stage[0] == '\0' ||
        result == NULL || result[0] == '\0') {
        return -1;
    }

    json_buf_init(&jb, wire_json, sizeof(wire_json));
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_EVENT_REPORT, 0U, upgrade_token, NULL) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"ec\":\"upg\"") != 0 ||
        append_optional_string_field(&jb, "ut", upgrade_token) != 0 ||
        append_optional_string_field(&jb, "stg", stage) != 0 ||
        append_optional_string_field(&jb, "res", result) != 0 ||
        append_u32_field(&jb, "pp", progress_percent) != 0) {
        return -2;
    }
    if ((upgrade_job_id != NULL && upgrade_job_id[0] != '\0' &&
         append_optional_string_field(&jb, "uj", upgrade_job_id) != 0) ||
        (upgrade_item_id != NULL && upgrade_item_id[0] != '\0' &&
         append_optional_string_field(&jb, "ui", upgrade_item_id) != 0) ||
        (release_id != NULL && release_id[0] != '\0' &&
         append_optional_string_field(&jb, "rid", release_id) != 0) ||
        (release_code != NULL && release_code[0] != '\0' &&
         append_optional_string_field(&jb, "rcd", release_code) != 0) ||
        (package_artifact_id != NULL && package_artifact_id[0] != '\0' &&
         append_optional_string_field(&jb, "aid", package_artifact_id) != 0) ||
        (reason_code != NULL && reason_code[0] != '\0' &&
         append_optional_string_field(&jb, "rc", reason_code) != 0) ||
        (message != NULL && message[0] != '\0' &&
         append_optional_string_field(&jb, "msg", message) != 0) ||
        (firmware_version != NULL && firmware_version[0] != '\0' &&
         append_optional_string_field(&jb, "fv", firmware_version) != 0) ||
        (checksum != NULL && checksum[0] != '\0' &&
         append_optional_string_field(&jb, "sum", checksum) != 0)) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return net_connectivity_send_json(wire_json, jb.len);
}
