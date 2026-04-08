#include "proto_register.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "common_identity.h"
#include "storage_config.h"

#include <stdio.h>
#include <string.h>

static int append_json_kv_u32(json_buf_t *jb, const char *k, uint32_t v)
{
    char tmp[48];
    (void)snprintf(tmp, sizeof(tmp), "\"%s\":%lu", k, (unsigned long)v);
    return json_buf_append(jb, tmp);
}

int proto_register_build(char *buf, size_t cap)
{
    if (!buf || cap < 512U) {
        return -1;
    }
    device_config_t cfg;
    uint32_t        cv = 0U;
    if (storage_config_load(&cfg) == 0) {
        cv = cfg.config_version;
    }
    const controller_identity_t *id = common_identity_get();
    const resource_inventory_t  *ri = common_resource_inventory_get();
    feature_modules_t            fm;
    memset(&fm, 0, sizeof(fm));
    if (storage_config_load(&cfg) == 0) {
        fm = cfg.feature_modules;
    }

    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_REGISTER, 0U, NULL, NULL) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"identity\":{") != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"iccid\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, id->iccid) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\",\"hardware_sku\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, id->hardware_sku) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\",\"hardware_rev\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, id->hardware_rev) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\",\"firmware_family\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, id->firmware_family) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\",\"firmware_version\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, id->firmware_version) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "},") != 0) {
        return -2;
    }
    if (append_json_kv_u32(&jb, "config_version", cv) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, ",\"resource_inventory\":{") != 0) {
        return -2;
    }
    char tmp[192];
    (void)snprintf(tmp, sizeof(tmp),
                   "\"ai_count\":%u,\"di_count\":%u,\"do_count\":%u,\"rs485_count\":%u,"
                   "\"relay_count\":%u,\"pulse_count\":%u,\"battery_monitor\":%u,"
                   "\"solar_monitor\":%u,\"signal_monitor\":%u",
                   (unsigned)ri->ai_count, (unsigned)ri->di_count, (unsigned)ri->do_count,
                   (unsigned)ri->rs485_count, (unsigned)ri->relay_count, (unsigned)ri->pulse_count,
                   (unsigned)ri->battery_monitor, (unsigned)ri->solar_monitor,
                   (unsigned)ri->signal_monitor);
    if (json_buf_append(&jb, tmp) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "},\"feature_modules\":[") != 0) {
        return -2;
    }
    uint8_t first_module = 1U;
#define APPEND_MODULE(enabled, code)                                    \
    do {                                                                \
        if ((enabled) != 0U) {                                          \
            if (!first_module && json_buf_append(&jb, ",") != 0) {      \
                return -2;                                              \
            }                                                           \
            if (json_buf_append(&jb, "\"") != 0) {                      \
                return -2;                                              \
            }                                                           \
            if (json_buf_append(&jb, (code)) != 0) {                    \
                return -2;                                              \
            }                                                           \
            if (json_buf_append(&jb, "\"") != 0) {                      \
                return -2;                                              \
            }                                                           \
            first_module = 0U;                                          \
        }                                                               \
    } while (0)
    APPEND_MODULE(fm.pump_vfd_control, "pump_vfd_control");
    APPEND_MODULE(fm.single_valve_control, "single_valve_control");
    APPEND_MODULE(fm.pressure_acquisition, "pressure_acquisition");
    APPEND_MODULE(fm.flow_acquisition, "flow_acquisition");
    APPEND_MODULE(fm.electric_meter_modbus, "electric_meter_modbus");
    APPEND_MODULE(fm.soil_moisture_acquisition, "soil_moisture_acquisition");
    APPEND_MODULE(fm.soil_temperature_acquisition, "soil_temperature_acquisition");
    APPEND_MODULE(fm.liquid_level_acquisition, "liquid_level_acquisition");
    APPEND_MODULE(fm.remote_io_extension, "remote_io_extension");
#undef APPEND_MODULE
    if (json_buf_append(&jb, "]") != 0) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}
