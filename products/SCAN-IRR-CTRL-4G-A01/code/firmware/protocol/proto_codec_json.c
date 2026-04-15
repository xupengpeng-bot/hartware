#include "proto_codec_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void json_buf_init(json_buf_t *b, char *mem, size_t cap)
{
    b->buf = mem;
    b->cap = cap;
    b->len = 0U;
    if (cap > 0U && mem) {
        mem[0] = '\0';
    }
}

int json_buf_append(json_buf_t *b, const char *s)
{
    if (!b || !b->buf || !s) {
        return -1;
    }
    size_t sl = strlen(s);
    if (b->len + sl + 1U > b->cap) {
        return -2;
    }
    memcpy(b->buf + b->len, s, sl + 1U);
    b->len += sl;
    return 0;
}

int json_buf_append_fmt(json_buf_t *b, const char *fmt, ...)
{
    if (!b || !b->buf || !fmt) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap > b->len ? b->cap - b->len : 0, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= (b->cap > b->len ? b->cap - b->len : 0)) {
        return -2;
    }
    b->len += (size_t)n;
    return 0;
}

int json_escape_append(json_buf_t *b, const char *s)
{
    if (!s) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        char esc[8];
        if (*p == '"' || *p == '\\') {
            esc[0] = '\\';
            esc[1] = (char)*p;
            esc[2] = '\0';
            if (json_buf_append(b, esc) != 0) {
                return -1;
            }
        } else if (*p < 0x20U) {
            (void)snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)*p);
            if (json_buf_append(b, esc) != 0) {
                return -1;
            }
        } else {
            esc[0] = (char)*p;
            esc[1] = '\0';
            if (json_buf_append(b, esc) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int json_buf_append_fixed(json_buf_t *b, float value, uint8_t frac_digits)
{
    uint32_t scale = 1U;
    int32_t  scaled;
    uint32_t abs_scaled;
    uint32_t int_part;
    uint32_t frac_part;
    char     tmp[32];

    if (b == NULL || b->buf == NULL) {
        return -1;
    }
    if (frac_digits > 3U) {
        frac_digits = 3U;
    }
    /* Clamp NaN / Inf / obviously broken runtime values to a safe numeric fallback. */
    if (!(value >= -1000000.0f && value <= 1000000.0f)) {
        value = 0.0f;
    }
    for (uint8_t i = 0U; i < frac_digits; i++) {
        scale *= 10U;
    }

    if (value >= 0.0f) {
        scaled = (int32_t)(value * (float)scale + 0.5f);
    } else {
        scaled = (int32_t)(value * (float)scale - 0.5f);
    }

    abs_scaled = (scaled < 0) ? (uint32_t)(-scaled) : (uint32_t)scaled;
    int_part = abs_scaled / scale;
    frac_part = abs_scaled % scale;

    if (frac_digits == 0U) {
        (void)snprintf(tmp, sizeof(tmp), "%s%lu",
                       scaled < 0 ? "-" : "",
                       (unsigned long)int_part);
    } else {
        (void)snprintf(tmp, sizeof(tmp), "%s%lu.%0*lu",
                       scaled < 0 ? "-" : "",
                       (unsigned long)int_part,
                       (int)frac_digits,
                       (unsigned long)frac_part);
    }
    return json_buf_append(b, tmp);
}

static const char *json_find_value_start(const char *json, const char *key)
{
    char pattern[48];
    const char *p;

    if (!json || !key) {
        return NULL;
    }
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return NULL;
    }
    p = strchr(p, ':');
    if (!p) {
        return NULL;
    }
    p++;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

int proto_json_get_string(const char *json, const char *key, char *out, size_t out_sz)
{
    const char *p;

    if (!json || !key || !out || out_sz == 0U) {
        return -1;
    }
    p = json_find_value_start(json, key);
    if (p == NULL) {
        out[0] = '\0';
        return -1;
    }
    if (*p != '"') {
        out[0] = '\0';
        return -1;
    }
    p++;
    size_t i = 0U;
    while (*p != '\0' && *p != '"' && i + 1U < out_sz) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

int proto_json_get_u32(const char *json, const char *key, uint32_t *out)
{
    const char *p;

    if (!json || !key || !out) {
        return -1;
    }
    p = json_find_value_start(json, key);
    if (p == NULL) {
        return -1;
    }
    *out = (uint32_t)strtoul(p, NULL, 10);
    return 0;
}

int proto_json_get_i32(const char *json, const char *key, int32_t *out)
{
    const char *p;

    if (!json || !key || !out) {
        return -1;
    }
    p = json_find_value_start(json, key);
    if (p == NULL) {
        return -1;
    }
    *out = (int32_t)strtol(p, NULL, 10);
    return 0;
}

int proto_json_get_float(const char *json, const char *key, float *out)
{
    const char *p;

    if (!json || !key || !out) {
        return -1;
    }
    p = json_find_value_start(json, key);
    if (p == NULL) {
        return -1;
    }
    *out = (float)strtod(p, NULL);
    return 0;
}

int proto_json_get_bool(const char *json, const char *key, uint8_t *out)
{
    const char *p;

    if (!json || !key || !out) {
        return -1;
    }
    p = json_find_value_start(json, key);
    if (p == NULL) {
        return -1;
    }
    if (strncmp(p, "true", 4) == 0 || *p == '1') {
        *out = 1U;
        return 0;
    }
    if (strncmp(p, "false", 5) == 0 || *p == '0') {
        *out = 0U;
        return 0;
    }
    return -1;
}

int proto_json_get_u8_01(const char *json, const char *key, uint8_t *out)
{
    uint8_t v = 0U;

    if (proto_json_get_bool(json, key, &v) == 0) {
        *out = v;
        return 0;
    }
    {
        uint32_t num = 0U;
        if (proto_json_get_u32(json, key, &num) != 0) {
            return -1;
        }
        *out = (num != 0U) ? 1U : 0U;
    }
    return 0;
}

typedef struct {
    const char *long_code;
    const char *short_code;
} proto_code_pair_t;

static const proto_code_pair_t s_reject_map[] = {
    { "DEVICE_BUSY", "BZ" },
    { "UNSUPPORTED_COMMAND", "UC" },
    { "INVALID_CHANNEL", "PI" },
    { "CHANNEL_DISABLED", "MN" },
    { "MODULE_NOT_ENABLED", "MN" },
    { "CAPABILITY_NOT_EXPOSED", "MN" },
    { "SAFETY_INTERLOCK", "SI" },
    { "LOW_BATTERY", "LB" },
    { "POWER_NOT_READY", "PR" },
    { "SENSOR_REQUIRED", "MN" },
    { "PARAM_INVALID", "PI" },
    { "CONFIG_VERSION_MISMATCH", "CV" },
    { "EXPIRED_COMMAND", "EX" }
};

static const proto_code_pair_t s_module_map[] = {
    { "pump_vfd_control", "pvc" },
    { "pump_direct_control", "pdc" },
    { "single_valve_control", "svl" },
    { "dual_valve_control", "dvl" },
    { "relay_output_control", "rly" },
    { "pressure_acquisition", "prs" },
    { "flow_acquisition", "flw" },
    { "soil_moisture_acquisition", "sma" },
    { "soil_temperature_acquisition", "sta" },
    { "power_monitoring", "pwm" },
    { "payment_qr_control", "pay" },
    { "card_auth_reader", "cdr" },
    { "electric_meter_modbus", "ebr" },
    { "valve_feedback_monitor", "vfb" },
    { "breaker_control", "bkr" },
    { "breaker_feedback_monitor", "bkf" },
    { "rs485_sensor_gateway", "rsg" },
    { "rs485_vfd_gateway", "rvg" },
    { "remote_start_enable", "rse" },
    { "auto_linkage_enable", "ale" },
    { "auto_stop_on_low_pressure", "alp" },
    { "auto_stop_on_high_pressure", "ahp" }
};

static const proto_code_pair_t s_workflow_map[] = {
    { "BOOTING", "RI" },
    { "NOT_READY", "RI" },
    { "ONLINE_NOT_READY", "RI" },
    { "AUTH_PENDING", "ST" },
    { "PRE_START_METERING", "ST" },
    { "START_SEQUENCE", "ST" },
    { "READY_IDLE", "RI" },
    { "RUNNING", "RN" },
    { "PAUSING", "PA" },
    { "PAUSED", "PS" },
    { "RESUMING", "RS" },
    { "STOP_SEQUENCE", "SP" },
    { "POST_STOP_METERING", "SP" },
    { "BLOCKED", "RI" },
    { "FAULT_LATCHED", "ER" },
    { "RECOVERY_LOCKED", "ER" },
    { "STOPPED", "ED" },
    { "ERROR_STOP", "ER" },
    { "booting", "RI" },
    { "not_ready", "RI" },
    { "online_not_ready", "RI" },
    { "auth_pending", "ST" },
    { "pre_start_metering", "ST" },
    { "start_sequence", "ST" },
    { "ready_idle", "RI" },
    { "running", "RN" },
    { "pausing", "PA" },
    { "paused", "PS" },
    { "resuming", "RS" },
    { "stop_sequence", "SP" },
    { "post_stop_metering", "SP" },
    { "blocked", "RI" },
    { "fault_latched", "ER" },
    { "recovery_locked", "ER" },
    { "stopped", "ED" },
    { "error_stop", "ER" }
};

static const proto_code_pair_t s_power_mode_map[] = {
    { "mains", "ac" },
    { "battery", "bat" },
    { "solar", "sol" },
    { "hybrid", "hyb" },
    { "unknown", "uk" }
};

static const char *map_long_to_short(const proto_code_pair_t *pairs, size_t count, const char *value)
{
    size_t i;

    if (value == NULL || value[0] == '\0') {
        return "";
    }
    for (i = 0U; i < count; i++) {
        if (strcmp(pairs[i].long_code, value) == 0 || strcmp(pairs[i].short_code, value) == 0) {
            return pairs[i].short_code;
        }
    }
    return value;
}

const char *proto_map_reject_short(const char *value)
{
    return map_long_to_short(s_reject_map, sizeof(s_reject_map) / sizeof(s_reject_map[0]), value);
}

const char *proto_map_module_short(const char *value)
{
    return map_long_to_short(s_module_map, sizeof(s_module_map) / sizeof(s_module_map[0]), value);
}

const char *proto_map_workflow_short(const char *value)
{
    return map_long_to_short(s_workflow_map, sizeof(s_workflow_map) / sizeof(s_workflow_map[0]), value);
}

const char *proto_map_workflow_short_from_runtime(const char *value, uint8_t ready_flag)
{
    const char *mapped = proto_map_workflow_short(value);

    if (mapped == NULL || mapped[0] == '\0') {
        return "ER";
    }
    if (strcmp(mapped, "RI") == 0 && ready_flag == 0U) {
        return "RI";
    }
    return mapped;
}

const char *proto_map_power_mode_short(const char *value)
{
    return map_long_to_short(s_power_mode_map, sizeof(s_power_mode_map) / sizeof(s_power_mode_map[0]), value);
}
