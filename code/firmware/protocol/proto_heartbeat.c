#include "proto_heartbeat.h"

#include "common_status.h"
#include "config_store.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "runtime_state.h"

#include <string.h>

static int append_string_field(json_buf_t *jb, const char *key, const char *value)
{
    if (jb == NULL || key == NULL || value == NULL) {
        return -1;
    }
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":\"") != 0 ||
        json_escape_append(jb, value) != 0 ||
        json_buf_append(jb, "\"") != 0) {
        return -1;
    }
    return 0;
}

static uint8_t metric_in_range(float value, float min_allowed, float max_allowed)
{
    return (value >= min_allowed && value <= max_allowed) ? 1U : 0U;
}

static int append_numeric_flag_field(json_buf_t *jb, const char *key, uint8_t value)
{
    if (jb == NULL || key == NULL) {
        return -1;
    }
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0) {
        return -1;
    }
    return json_buf_append_fmt(jb, "%u", value != 0U ? 1U : 0U);
}

static int append_u32_field(json_buf_t *jb, const char *key, uint32_t value)
{
    if (jb == NULL || key == NULL) {
        return -1;
    }
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0) {
        return -1;
    }
    return json_buf_append_fmt(jb, "%lu", (unsigned long)value);
}

static int append_i32_field(json_buf_t *jb, const char *key, int32_t value)
{
    if (jb == NULL || key == NULL) {
        return -1;
    }
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0) {
        return -1;
    }
    return json_buf_append_fmt(jb, "%ld", (long)value);
}

static int append_fixed_field(json_buf_t *jb, const char *key, float value, uint8_t frac_digits)
{
    if (jb == NULL || key == NULL) {
        return -1;
    }
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0) {
        return -1;
    }
    return json_buf_append_fixed(jb, value, frac_digits);
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

static int append_optional_separator(json_buf_t *jb, uint8_t *first)
{
    if (jb == NULL || first == NULL) {
        return -1;
    }
    if (*first == 0U) {
        return json_buf_append(jb, ",");
    }
    *first = 0U;
    return 0;
}

static int heartbeat_build_common(char *buf, size_t cap, uint32_t seq)
{
    json_buf_t jb;
    const common_status_t *cs = common_status_get();
    const runtime_state_t *rs = runtime_state_get();
    uint8_t signal_valid;
    uint8_t battery_v_valid;
    uint8_t solar_v_valid;
    uint8_t battery_soc_valid;
    uint8_t first = 1U;

    if (buf == NULL || cap < 512U || rs == NULL || cs == NULL) {
        return -1;
    }

    signal_valid = (rs->signal_csq >= 0 && rs->signal_csq <= 99) ? 1U : 0U;
    battery_v_valid = metric_in_range(cs->battery_voltage_v, 0.1f, 64.0f);
    solar_v_valid = metric_in_range(cs->solar_voltage_v, 0.0f, 64.0f);
    battery_soc_valid = (battery_v_valid != 0U && rs->battery_soc <= 100U) ? 1U : 0U;

    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_HEARTBEAT, seq, NULL, NULL) != 0) {
        return -2;
    }

    if (append_optional_separator(&jb, &first) != 0 ||
        append_numeric_flag_field(&jb, "rd", rs->ready ? 1U : 0U) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_numeric_flag_field(&jb, "on", rs->online ? 1U : 0U) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_numeric_flag_field(&jb, "tc", rs->tcp_connected ? 1U : 0U) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_string_field(&jb, "wf",
                            proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                  rs->ready ? 1U : 0U)) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_u32_field(&jb, "cv", rs->config_version) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_string_field(&jb, "pm", proto_map_power_mode_short(runtime_state_power_name(rs->power_state))) != 0) {
        return -3;
    }

    if (append_optional_separator(&jb, &first) != 0 ||
        append_u32_field(&jb, "rt", rs->runtime_sec) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_u32_field(&jb, "me", rs->meter_epoch) != 0) {
        return -3;
    }

    if (metric_in_range(rs->total_m3, 0.0f, 10000000.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "fq", rs->total_m3, 2U) != 0) {
            return -3;
        }
    }
    if (metric_in_range(rs->energy_kwh, 0.0f, 10000000.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "ek", rs->energy_kwh, 2U) != 0) {
            return -3;
        }
    }

    if (signal_valid != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_i32_field(&jb, "csq", rs->signal_csq) != 0) {
            return -3;
        }
    }
    if (battery_soc_valid != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_u32_field(&jb, "bs", (uint32_t)rs->battery_soc) != 0) {
            return -3;
        }
    }
    if (battery_v_valid != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "bv", cs->battery_voltage_v, 2U) != 0) {
            return -3;
        }
    }
    if (solar_v_valid != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "sv", cs->solar_voltage_v, 2U) != 0) {
            return -3;
        }
    }
    if (breaker_state_value(rs) != NULL) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_string_field(&jb, "brs", breaker_state_value(rs)) != 0) {
            return -3;
        }
    }

    if (proto_envelope_close_payload(&jb) != 0) {
        return -4;
    }
    return (int)jb.len;
}

int proto_heartbeat_build(char *buf, size_t cap)
{
    return heartbeat_build_common(buf, cap, 0U);
}
