#include "proto_register.h"

#include "common_identity.h"
#include "config_store.h"
#include "module_meter.h"
#include "proto_capability.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *module_code;
    size_t field_offset;
} capability_desc_t;

static const capability_desc_t s_capability_defs[] = {
    { "pump_vfd_control",      offsetof(feature_modules_t, pump_vfd_control) },
    { "pump_direct_control",   offsetof(feature_modules_t, pump_direct_control) },
    { "single_valve_control",  offsetof(feature_modules_t, single_valve_control) },
    { "pressure_acquisition",  offsetof(feature_modules_t, pressure_acquisition) },
    { "flow_acquisition",      offsetof(feature_modules_t, flow_acquisition) },
    { "soil_moisture_acquisition", offsetof(feature_modules_t, soil_moisture_acquisition) },
    { "soil_temperature_acquisition", offsetof(feature_modules_t, soil_temperature_acquisition) },
    { "power_monitoring",      offsetof(feature_modules_t, power_monitoring) },
    { "payment_qr_control",    offsetof(feature_modules_t, payment_qr_control) },
    { "card_auth_reader",      offsetof(feature_modules_t, card_auth_reader) },
    { "electric_meter_modbus", offsetof(feature_modules_t, electric_meter_modbus) },
    { "valve_feedback_monitor", offsetof(feature_modules_t, valve_feedback_monitor) },
    { "breaker_control",       offsetof(feature_modules_t, breaker_control) },
    { "breaker_feedback_monitor", offsetof(feature_modules_t, breaker_feedback_monitor) },
    { "rs485_sensor_gateway",  offsetof(feature_modules_t, rs485_sensor_gateway) },
    { "rs485_vfd_gateway",     offsetof(feature_modules_t, rs485_vfd_gateway) },
    { "remote_start_enable",   offsetof(feature_modules_t, remote_start_enable) },
    { "auto_linkage_enable",   offsetof(feature_modules_t, auto_linkage_enable) },
    { "auto_stop_on_low_pressure", offsetof(feature_modules_t, auto_stop_on_low_pressure) },
    { "auto_stop_on_high_pressure", offsetof(feature_modules_t, auto_stop_on_high_pressure) }
};

static int append_string_field(json_buf_t *jb, const char *key, const char *value, uint8_t *first)
{
    if (jb == NULL || key == NULL || value == NULL || first == NULL) {
        return -1;
    }
    if (*first == 0U && json_buf_append(jb, ",") != 0) {
        return -1;
    }
    *first = 0U;
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":\"") != 0 ||
        json_escape_append(jb, value) != 0 ||
        json_buf_append(jb, "\"") != 0) {
        return -1;
    }
    return 0;
}

static int append_u32_field(json_buf_t *jb, const char *key, uint32_t value, uint8_t *first)
{
    if (jb == NULL || key == NULL || first == NULL) {
        return -1;
    }
    if (*first == 0U && json_buf_append(jb, ",") != 0) {
        return -1;
    }
    *first = 0U;
    return json_buf_append_fmt(jb, "\"%s\":%lu", key, (unsigned long)value);
}

static int append_raw_json_field(json_buf_t *jb, const char *key, const char *value, uint8_t *first)
{
    if (jb == NULL || key == NULL || value == NULL || first == NULL) {
        return -1;
    }
    if (*first == 0U && json_buf_append(jb, ",") != 0) {
        return -1;
    }
    *first = 0U;
    if (json_buf_append(jb, "\"") != 0 ||
        json_buf_append(jb, key) != 0 ||
        json_buf_append(jb, "\":") != 0 ||
        json_buf_append(jb, value) != 0) {
        return -1;
    }
    return 0;
}

static void fill_default_feature_modules(feature_modules_t *fm)
{
    if (fm == NULL) {
        return;
    }
    memset(fm, 0, sizeof(*fm));
    fm->payment_qr_control = 1U;
    fm->card_auth_reader = 1U;
    fm->electric_meter_modbus = 1U;
    fm->breaker_control = 1U;
}

static void filter_real_feature_modules(feature_modules_t *dst, const feature_modules_t *src)
{
    if (dst == NULL) {
        return;
    }
    if (src == NULL) {
        memset(dst, 0, sizeof(*dst));
        return;
    }
    *dst = *src;
}

static const char *register_meter_protocol(const feature_modules_t *fm)
{
    if (fm == NULL || fm->electric_meter_modbus == 0U) {
        return NULL;
    }
    return module_meter_source_name();
}

static const char *register_control_protocol(const device_config_t *cfg, const feature_modules_t *fm)
{
    if (cfg == NULL || fm == NULL || fm->breaker_control == 0U) {
        return NULL;
    }
    switch (cfg->control_config.pump_control_mode) {
    case PUMP_CONTROL_METER_BREAKER_485:
        return register_meter_protocol(fm);
    case PUMP_CONTROL_CONTACTOR_DIRECT:
        return "contactor_direct";
    case PUMP_CONTROL_RELAY_DIRECT:
    default:
        return "relay_direct";
    }
}

static uint8_t feature_enabled(const feature_modules_t *fm, size_t field_offset)
{
    const uint8_t *base;

    if (fm == NULL) {
        return 0U;
    }
    base = (const uint8_t *)fm;
    return *(const uint8_t *)(base + field_offset) != 0U ? 1U : 0U;
}

static int append_feature_modules(json_buf_t *jb, const feature_modules_t *fm, uint8_t *first)
{
    uint8_t emitted = 0U;
    size_t i;

    if (jb == NULL || fm == NULL || first == NULL) {
        return -1;
    }
    if (*first == 0U && json_buf_append(jb, ",") != 0) {
        return -1;
    }
    *first = 0U;
    if (json_buf_append(jb, "\"fm\":[") != 0) {
        return -1;
    }
    for (i = 0U; i < sizeof(s_capability_defs) / sizeof(s_capability_defs[0]); i++) {
        if (feature_enabled(fm, s_capability_defs[i].field_offset) == 0U) {
            continue;
        }
        if (emitted != 0U && json_buf_append(jb, ",") != 0) {
            return -1;
        }
        emitted = 1U;
        if (json_buf_append(jb, "\"") != 0 ||
            json_buf_append(jb, proto_map_module_short(s_capability_defs[i].module_code)) != 0 ||
            json_buf_append(jb, "\"") != 0) {
            return -1;
        }
    }
    return json_buf_append(jb, "]");
}

int proto_register_build(char *buf, size_t cap)
{
    json_buf_t jb;
    const controller_identity_t *id;
    const device_config_t *cfg;
    proto_capability_info_t capability;
    feature_modules_t fm;
    uint32_t config_version = 1U;
    uint8_t first = 1U;

    if (buf == NULL || cap < 512U) {
        return -1;
    }

    id = common_identity_get();
    cfg = config_store_active();
    if (id == NULL) {
        return -2;
    }

    fill_default_feature_modules(&fm);
    if (cfg != NULL) {
        filter_real_feature_modules(&fm, &cfg->feature_modules);
        config_version = cfg->config_version;
    }
    if (cfg == NULL || proto_capability_describe(cfg, &fm, &capability) != 0) {
        return -2;
    }

    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_REGISTER, 0U, NULL, NULL) != 0) {
        return -3;
    }

    if (append_string_field(&jb, "hs", id->hardware_sku, &first) != 0 ||
        append_string_field(&jb, "hr", id->hardware_rev, &first) != 0 ||
        append_string_field(&jb, "ff", id->firmware_family, &first) != 0 ||
        append_string_field(&jb, "fv", id->firmware_version, &first) != 0 ||
        append_u32_field(&jb, "cv", config_version, &first) != 0 ||
        append_feature_modules(&jb, &fm, &first) != 0 ||
        append_u32_field(&jb, "cap_ver", capability.capability_version, &first) != 0 ||
        append_string_field(&jb, "cap_hash", capability.capability_hash, &first) != 0 ||
        append_string_field(&jb, "config_bitmap", capability.config_bitmap_hex, &first) != 0 ||
        append_string_field(&jb, "actions_bitmap", capability.actions_bitmap_hex, &first) != 0 ||
        append_string_field(&jb, "queries_bitmap", capability.queries_bitmap_hex, &first) != 0 ||
        append_raw_json_field(&jb, "limits", capability.limits_json, &first) != 0) {
        return -4;
    }
    if (register_meter_protocol(&fm) != NULL &&
        append_string_field(&jb, "mp", register_meter_protocol(&fm), &first) != 0) {
        return -4;
    }
    if (register_control_protocol(cfg, &fm) != NULL &&
        append_string_field(&jb, "cp", register_control_protocol(cfg, &fm), &first) != 0) {
        return -4;
    }

    if (proto_envelope_close_payload(&jb) != 0) {
        return -5;
    }
    return (int)jb.len;
}
