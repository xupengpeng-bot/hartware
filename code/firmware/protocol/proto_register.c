#include "proto_register.h"
#include "proto_json_builder.h"
#include "proto_envelope.h"
#include "common_identity.h"
#include "storage_config.h"
#include "cJSON.h"

#include <string.h>

static int require_nonempty_field(const char *value)
{
    return (value != NULL && value[0] != '\0') ? 0 : -1;
}

static cJSON *build_resource_inventory_json(const resource_inventory_t *ri)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    if (cJSON_AddNumberToObject(obj, "ai_count", (double)ri->ai_count) == NULL ||
        cJSON_AddNumberToObject(obj, "di_count", (double)ri->di_count) == NULL ||
        cJSON_AddNumberToObject(obj, "do_count", (double)ri->do_count) == NULL ||
        cJSON_AddNumberToObject(obj, "rs485_count", (double)ri->rs485_count) == NULL ||
        cJSON_AddNumberToObject(obj, "relay_count", (double)ri->relay_count) == NULL ||
        cJSON_AddNumberToObject(obj, "pulse_count", (double)ri->pulse_count) == NULL ||
        cJSON_AddNumberToObject(obj, "battery_monitor", (double)ri->battery_monitor) == NULL ||
        cJSON_AddNumberToObject(obj, "solar_monitor", (double)ri->solar_monitor) == NULL ||
        cJSON_AddNumberToObject(obj, "signal_monitor", (double)ri->signal_monitor) == NULL) {
        cJSON_Delete(obj);
        return NULL;
    }
    return obj;
}

static cJSON *build_feature_modules_json(const feature_modules_t *fm)
{
    cJSON *arr = cJSON_CreateArray();
    if (arr == NULL) {
        return NULL;
    }
#define APPEND_MODULE(enabled, code) \
    do { \
        if ((enabled) != 0U) { \
            cJSON *it = cJSON_CreateString((code)); \
            if (it == NULL) { cJSON_Delete(arr); return NULL; } \
            cJSON_AddItemToArray(arr, it); \
        } \
    } while (0)
    APPEND_MODULE(fm->pump_vfd_control, "pump_vfd_control");
    APPEND_MODULE(fm->single_valve_control, "single_valve_control");
    APPEND_MODULE(fm->pressure_acquisition, "pressure_acquisition");
    APPEND_MODULE(fm->flow_acquisition, "flow_acquisition");
    APPEND_MODULE(fm->electric_meter_modbus, "electric_meter_modbus");
    APPEND_MODULE(fm->soil_moisture_acquisition, "soil_moisture_acquisition");
    APPEND_MODULE(fm->soil_temperature_acquisition, "soil_temperature_acquisition");
    APPEND_MODULE(fm->liquid_level_acquisition, "liquid_level_acquisition");
    APPEND_MODULE(fm->remote_io_extension, "remote_io_extension");
#undef APPEND_MODULE
    return arr;
}

int proto_register_build(char *buf, size_t cap)
{
    uint32_t cv = 0U;
    int tzq = MODEL_DEFAULT_TIME_ZONE_QUARTER_HOURS;
    feature_modules_t fm;
    const device_config_t *cfg;
    const controller_identity_t *id;
    const resource_inventory_t *ri;
    cJSON *payload = NULL;
    cJSON *identity = NULL;
    cJSON *inventory = NULL;
    cJSON *features = NULL;
    int rc;

    if (buf == NULL || cap < 512U) {
        return -1;
    }
    memset(&fm, 0, sizeof(fm));
    cfg = storage_config_active();
    if (cfg != NULL) {
        cv = cfg->config_version;
        fm = cfg->feature_modules;
        if (cfg->time_zone_quarter_hours >= -48 && cfg->time_zone_quarter_hours <= 56) {
            tzq = cfg->time_zone_quarter_hours;
        }
    }
    id = common_identity_get();
    ri = common_resource_inventory_get();
    if (id == NULL || ri == NULL) {
        return -2;
    }
    if (require_nonempty_field(id->iccid) != 0 || require_nonempty_field(id->imei) != 0 ||
        require_nonempty_field(id->hardware_sku) != 0 || require_nonempty_field(id->hardware_rev) != 0 ||
        require_nonempty_field(id->firmware_family) != 0 || require_nonempty_field(id->firmware_version) != 0) {
        return -3;
    }

    payload = cJSON_CreateObject();
    identity = cJSON_CreateObject();
    inventory = build_resource_inventory_json(ri);
    features = build_feature_modules_json(&fm);
    if (payload == NULL || identity == NULL || inventory == NULL || features == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(identity);
        cJSON_Delete(inventory);
        cJSON_Delete(features);
        return -4;
    }

    if (cJSON_AddStringToObject(identity, "iccid", id->iccid) == NULL ||
        cJSON_AddStringToObject(identity, "hardware_sku", id->hardware_sku) == NULL ||
        cJSON_AddStringToObject(identity, "hardware_rev", id->hardware_rev) == NULL ||
        cJSON_AddStringToObject(identity, "firmware_family", id->firmware_family) == NULL ||
        cJSON_AddStringToObject(identity, "firmware_version", id->firmware_version) == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(identity);
        cJSON_Delete(inventory);
        cJSON_Delete(features);
        return -5;
    }

    cJSON_AddItemToObject(payload, "identity", identity);
    cJSON_AddNumberToObject(payload, "config_version", (double)cv);
    cJSON_AddNumberToObject(payload, "time_zone_quarter_hours", (double)tzq);
    cJSON_AddItemToObject(payload, "resource_inventory", inventory);
    cJSON_AddItemToObject(payload, "feature_modules", features);

    rc = proto_json_build_message(buf, cap, PROTO_MSG_REGISTER, 0U, NULL, NULL, payload);
    if (rc < 0) {
        return rc;
    }
    return rc;
}
