#include "proto_sync_config.h"

#include "config_store.h"
#include "proto_codec_json.h"
#include "storage_config.h"

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

static void cfg_log(const char *msg)
{
    bsp_debug_log("[CFG] ");
    bsp_debug_log(msg);
    bsp_debug_log("\r\n");
}

static int json_read_u32(const cJSON *obj, const char *key, uint32_t *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);

    if (!cJSON_IsNumber(item) || out == NULL) {
        return -1;
    }
    *out = (uint32_t)item->valuedouble;
    return 0;
}

static int json_read_u8_01(const cJSON *obj, const char *key, uint8_t *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);

    if (out == NULL || item == NULL) {
        return -1;
    }
    if (cJSON_IsBool(item)) {
        *out = cJSON_IsTrue(item) ? 1U : 0U;
        return 0;
    }
    if (cJSON_IsNumber(item)) {
        *out = (item->valuedouble != 0.0) ? 1U : 0U;
        return 0;
    }
    return -1;
}

static int json_read_float(const cJSON *obj, const char *key, float *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);

    if (!cJSON_IsNumber(item) || out == NULL) {
        return -1;
    }
    *out = (float)item->valuedouble;
    return 0;
}

static int json_read_string(const cJSON *obj, const char *key, char *out, size_t out_cap)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);

    if (!cJSON_IsString(item) || item->valuestring == NULL || out == NULL || out_cap == 0U) {
        return -1;
    }
    (void)snprintf(out, out_cap, "%s", item->valuestring);
    return 0;
}

static const cJSON *json_get_item_alias(const cJSON *obj, const char *primary_key, const char *alias_key)
{
    const cJSON *item;

    if (obj == NULL) {
        return NULL;
    }
    item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, primary_key);
    if (item == NULL && alias_key != NULL) {
        item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, alias_key);
    }
    return item;
}

static int json_read_u32_alias(const cJSON *obj, const char *primary_key, const char *alias_key, uint32_t *out)
{
    const cJSON *item = json_get_item_alias(obj, primary_key, alias_key);

    if (!cJSON_IsNumber(item) || out == NULL) {
        return -1;
    }
    *out = (uint32_t)item->valuedouble;
    return 0;
}

static int parse_feature_modules_array(const cJSON *arr, feature_modules_t *fm)
{
    const cJSON *item;

    if (!cJSON_IsArray(arr) || fm == NULL) {
        return -1;
    }
    memset(fm, 0, sizeof(*fm));
    cJSON_ArrayForEach(item, arr) {
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
            return -1;
        }
        if (strcmp(item->valuestring, "pay") == 0) {
            fm->payment_qr_control = 1U;
        } else if (strcmp(item->valuestring, "cdr") == 0) {
            fm->card_auth_reader = 1U;
        } else if (strcmp(item->valuestring, "ebr") == 0) {
            fm->electric_meter_modbus = 1U;
        } else if (strcmp(item->valuestring, "bkr") == 0) {
            fm->breaker_control = 1U;
        } else if (strcmp(item->valuestring, "bkf") == 0) {
            fm->breaker_feedback_monitor = 1U;
        } else if (strcmp(item->valuestring, "svl") == 0) {
            fm->single_valve_control = 1U;
        } else if (strcmp(item->valuestring, "pwm") == 0) {
            fm->power_monitoring = 1U;
        } else if (strcmp(item->valuestring, "prs") == 0) {
            fm->pressure_acquisition = 1U;
        } else if (strcmp(item->valuestring, "flw") == 0) {
            fm->flow_acquisition = 1U;
        } else if (strcmp(item->valuestring, "sma") == 0) {
            fm->soil_moisture_acquisition = 1U;
        } else if (strcmp(item->valuestring, "sta") == 0) {
            fm->soil_temperature_acquisition = 1U;
        } else if (strcmp(item->valuestring, "pvc") == 0) {
            fm->pump_vfd_control = 1U;
        } else if (strcmp(item->valuestring, "pdc") == 0) {
            fm->pump_direct_control = 1U;
        } else if (strcmp(item->valuestring, "vfb") == 0) {
            fm->valve_feedback_monitor = 1U;
        } else if (strcmp(item->valuestring, "rsg") == 0) {
            fm->rs485_sensor_gateway = 1U;
        } else if (strcmp(item->valuestring, "rvg") == 0) {
            fm->rs485_vfd_gateway = 1U;
        } else if (strcmp(item->valuestring, "rse") == 0) {
            fm->remote_start_enable = 1U;
        } else if (strcmp(item->valuestring, "ale") == 0) {
            fm->auto_linkage_enable = 1U;
        } else if (strcmp(item->valuestring, "alp") == 0) {
            fm->auto_stop_on_low_pressure = 1U;
        } else if (strcmp(item->valuestring, "ahp") == 0) {
            fm->auto_stop_on_high_pressure = 1U;
        }
    }
    return 0;
}

static void parse_runtime_rules(const cJSON *obj, runtime_rules_t *rules)
{
    uint32_t value = 0U;

    if (!cJSON_IsObject(obj) || rules == NULL) {
        return;
    }
    if (json_read_u32(obj, "heartbeat_interval_sec", &value) == 0) {
        rules->heartbeat_interval_sec = (uint16_t)(value > 0xFFFFU ? 0xFFFFU : value);
    }
    if (json_read_u32(obj, "snapshot_idle_interval_sec", &value) == 0) {
        rules->snapshot_idle_interval_sec = (uint16_t)(value > 0xFFFFU ? 0xFFFFU : value);
    }
    if (json_read_u32(obj, "snapshot_running_interval_sec", &value) == 0) {
        rules->snapshot_running_interval_sec = (uint16_t)(value > 0xFFFFU ? 0xFFFFU : value);
    }
    if (json_read_u32(obj, "cloud_auth_timeout_ms", &value) == 0) {
        rules->cloud_auth_timeout_ms = value;
    }
    if (json_read_u32(obj, "workflow_enabled", &value) == 0) {
        rules->workflow_enabled = (uint8_t)(value != 0U ? 1U : 0U);
    }
}

static void parse_protection_config(const cJSON *obj, protection_config_t *pc)
{
    uint32_t value_u32 = 0U;
    float value_f = 0.0f;

    if (!cJSON_IsObject(obj) || pc == NULL) {
        return;
    }
    (void)json_read_u8_01(obj, "overload_protection", &pc->overload_protection);
    (void)json_read_u8_01(obj, "phase_loss_protection", &pc->phase_loss_protection);
    (void)json_read_u8_01(obj, "under_voltage_protection", &pc->under_voltage_protection);
    (void)json_read_u8_01(obj, "over_voltage_protection", &pc->over_voltage_protection);
    (void)json_read_u8_01(obj, "dry_run_protection", &pc->dry_run_protection);
    if (json_read_float(obj, "over_current_limit_a", &value_f) == 0) {
        pc->over_current_limit_a = value_f;
    }
    if (json_read_float(obj, "under_voltage_limit_v", &value_f) == 0) {
        pc->under_voltage_limit_v = value_f;
    }
    if (json_read_float(obj, "over_voltage_limit_v", &value_f) == 0) {
        pc->over_voltage_limit_v = value_f;
    }
    if (json_read_float(obj, "pressure_high_limit", &value_f) == 0) {
        pc->pressure_high_limit = value_f;
    }
    if (json_read_float(obj, "pressure_low_limit", &value_f) == 0) {
        pc->pressure_low_limit = value_f;
    }
    if (json_read_u32(obj, "start_delay_ms", &value_u32) == 0) {
        pc->start_delay_ms = value_u32;
    }
    if (json_read_u32(obj, "stop_delay_ms", &value_u32) == 0) {
        pc->stop_delay_ms = value_u32;
    }
}

static void parse_control_config(const cJSON *obj, control_config_t *cc)
{
    char text[40];
    uint32_t value_u32 = 0U;

    if (!cJSON_IsObject(obj) || cc == NULL) {
        return;
    }
    if (json_read_string(obj, "pump_control_mode", text, sizeof(text)) == 0) {
        if (strcmp(text, "meter_breaker_485") == 0) {
            cc->pump_control_mode = PUMP_CONTROL_METER_BREAKER_485;
        } else if (strcmp(text, "contactor_direct") == 0) {
            cc->pump_control_mode = PUMP_CONTROL_CONTACTOR_DIRECT;
        } else {
            cc->pump_control_mode = PUMP_CONTROL_RELAY_DIRECT;
        }
    }
    if (json_read_string(obj, "valve_control_mode", text, sizeof(text)) == 0) {
        cc->valve_control_mode = strcmp(text, "relay_output") == 0 ? VALVE_CONTROL_RELAY_OUTPUT : VALVE_CONTROL_DIRECT_OUTPUT;
    }
    if (json_read_string(obj, "linkage_mode", text, sizeof(text)) == 0) {
        cc->linkage_mode = strcmp(text, "platform_orchestrated") == 0 ? LINKAGE_PLATFORM_ORCHESTRATED : LINKAGE_LOCAL_INTEGRATED;
    }
    if (json_read_string(obj, "valve_fail_safe_mode", text, sizeof(text)) == 0) {
        if (strcmp(text, "fail_open") == 0) {
            cc->valve_fail_safe_mode = VALVE_FAIL_OPEN;
        } else if (strcmp(text, "hold_last") == 0) {
            cc->valve_fail_safe_mode = VALVE_HOLD_LAST;
        } else {
            cc->valve_fail_safe_mode = VALVE_FAIL_CLOSE;
        }
    }
    if (json_read_string(obj, "pump_output_fail_safe", text, sizeof(text)) == 0) {
        cc->pump_output_fail_safe = strcmp(text, "hold_last") == 0 ? PUMP_FAIL_SAFE_HOLD_LAST : PUMP_FAIL_SAFE_DEENERGIZE;
    }
    (void)json_read_u8_01(obj, "offline_new_start_enabled", &cc->offline_new_start_enabled);
    (void)json_read_u8_01(obj, "power_restore_resume_enabled", &cc->power_restore_resume_enabled);
    (void)json_read_u8_01(obj, "cross_device_linkage_from_edge", &cc->cross_device_linkage_from_edge);
    if (json_read_u32(obj, "offline_max_runtime_sec", &value_u32) == 0) {
        cc->offline_max_runtime_sec = (uint16_t)(value_u32 > 0xFFFFU ? 0xFFFFU : value_u32);
    }
}

static int validate_config(const device_config_t *cfg)
{
    if (cfg == NULL || cfg->config_version == 0U) {
        return -1;
    }
    if (cfg->protection_config.pressure_high_limit > 0.0f &&
        cfg->protection_config.pressure_low_limit > 0.0f &&
        cfg->protection_config.pressure_low_limit >= cfg->protection_config.pressure_high_limit) {
        return -2;
    }
    return 0;
}

int proto_sync_config_apply(const char *json, size_t json_len, char *ack_json, size_t ack_cap)
{
    cJSON *root = NULL;
    cJSON *payload = NULL;
    char repaired_json[2049];
    const cJSON *item = NULL;
    device_config_t *cfg;
    const device_config_t *prev;
    uint32_t config_version = 0U;
    int rc;
    size_t repaired_len = 0U;

    (void)json_len;

    if (json == NULL) {
        return -1;
    }

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
                bsp_debug_log("[PROTO] SC repaired duplicate separators in inbound json\r\n");
            }
        }
    }
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        log_json_parse_failure("SC", json, json_len);
        return -2;
    }
    payload = cJSON_GetObjectItemCaseSensitive(root, "p");
    if (!cJSON_IsObject(payload)) {
        cJSON_Delete(root);
        cfg_log("reject sync_config: invalid payload");
        return -4;
    }

    if (json_read_u32_alias(payload, "cv", "config_version", &config_version) != 0 || config_version == 0U) {
        cJSON_Delete(root);
        cfg_log("reject sync_config: missing cv");
        return -3;
    }

    cfg = storage_config_inactive_mutable();
    if (cfg == NULL) {
        cJSON_Delete(root);
        return -2;
    }
    prev = config_store_active();
    if (prev != NULL) {
        *cfg = *prev;
    } else {
        memset(cfg, 0, sizeof(*cfg));
    }
    cfg->config_version = config_version;

    item = json_get_item_alias(payload, "fm", "feature_modules");
    if (item != NULL) {
        feature_modules_t fm;
        if (parse_feature_modules_array(item, &fm) != 0) {
            cJSON_Delete(root);
            cfg_log("reject sync_config: invalid fm");
            return -4;
        }
        cfg->feature_modules = fm;
    }

    item = json_get_item_alias(payload, "rr", "runtime_rules");
    if (item != NULL && !cJSON_IsObject(item)) {
        cJSON_Delete(root);
        return -4;
    }
    parse_runtime_rules(item, &cfg->runtime_rules);

    item = json_get_item_alias(payload, "pc", "protection_config");
    if (item != NULL && !cJSON_IsObject(item)) {
        cJSON_Delete(root);
        return -4;
    }
    parse_protection_config(item, &cfg->protection_config);

    item = json_get_item_alias(payload, "cc", "control_config");
    if (item != NULL && !cJSON_IsObject(item)) {
        cJSON_Delete(root);
        return -4;
    }
    parse_control_config(item, &cfg->control_config);

    rc = validate_config(cfg);
    if (rc != 0) {
        cJSON_Delete(root);
        cfg_log("reject sync_config: validation failed");
        return -5;
    }
    if (storage_config_commit_swap(cfg->config_version) != 0) {
        cJSON_Delete(root);
        cfg_log("reject sync_config: commit failed");
        return -6;
    }

    cJSON_Delete(root);

    if (ack_json != NULL && ack_cap > 0U) {
        (void)snprintf(ack_json, ack_cap, "\"cv\":%lu", (unsigned long)cfg->config_version);
    }
    {
        char line[96];
        (void)snprintf(line, sizeof(line), "applied cv=%lu", (unsigned long)cfg->config_version);
        cfg_log(line);
    }
    return 0;
}
