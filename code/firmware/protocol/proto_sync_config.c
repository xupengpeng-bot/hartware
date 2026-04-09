#include "proto_sync_config.h"
#include "proto_codec_json.h"
#include "model_config.h"
#include "storage_config.h"
#include <string.h>
#include <stdio.h>

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

static int copy_json_object(const char *start, char *out, size_t out_cap, const char **end_out)
{
    if (*start != '{') {
        return -1;
    }
    int depth = 0;
    const char *q = start;
    for (; *q; q++) {
        if (*q == '{') {
            depth++;
        } else if (*q == '}') {
            depth--;
            if (depth == 0) {
                q++;
                break;
            }
        }
    }
    size_t n = (size_t)(q - start);
    if (n + 1U > out_cap) {
        return -2;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    if (end_out) {
        *end_out = q;
    }
    return 0;
}

static int extract_object_for_key(const char *json, const char *key, char *out, size_t out_cap)
{
    char pat[56];
    (void)snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p = skip_ws(p + 1);
    if (*p != '{') {
        return -1;
    }
    return copy_json_object(p, out, out_cap, NULL);
}

static int parse_features(const char *json, feature_modules_t *fm)
{
    (void)memset(fm, 0, sizeof(*fm));
    (void)proto_json_get_u8_01(json, "pump_vfd_control", &fm->pump_vfd_control);
    (void)proto_json_get_u8_01(json, "single_valve_control", &fm->single_valve_control);
    (void)proto_json_get_u8_01(json, "pressure_acquisition", &fm->pressure_acquisition);
    (void)proto_json_get_u8_01(json, "flow_acquisition", &fm->flow_acquisition);
    (void)proto_json_get_u8_01(json, "electric_meter_modbus", &fm->electric_meter_modbus);
    (void)proto_json_get_u8_01(json, "soil_moisture_acquisition", &fm->soil_moisture_acquisition);
    (void)proto_json_get_u8_01(json, "soil_temperature_acquisition", &fm->soil_temperature_acquisition);
    (void)proto_json_get_u8_01(json, "liquid_level_acquisition", &fm->liquid_level_acquisition);
    (void)proto_json_get_u8_01(json, "remote_io_extension", &fm->remote_io_extension);
    return 0;
}

static int parse_features_nested(const char *json, feature_modules_t *fm)
{
    char buf[512];
    if (extract_object_for_key(json, "feature_modules", buf, sizeof(buf)) == 0) {
        return parse_features(buf, fm);
    }
    return parse_features(json, fm);
}

static int parse_runtime_rules(const char *json, runtime_rules_t *r)
{
    (void)memset(r, 0, sizeof(*r));
    uint32_t v = 0U;
    if (proto_json_get_u32(json, "heartbeat_interval_sec", &v) == 0) {
        r->heartbeat_interval_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "link_ping_interval_sec", &v) == 0) {
        r->link_ping_interval_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "vitals_interval_sec", &v) == 0) {
        r->vitals_interval_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "vitals_csq_delta", &v) == 0) {
        r->vitals_csq_delta = (uint8_t)(v > 255U ? 255U : v);
    }
    if (proto_json_get_u32(json, "vitals_soc_delta", &v) == 0) {
        r->vitals_soc_delta = (uint8_t)(v > 255U ? 255U : v);
    }
    if (proto_json_get_u32(json, "snapshot_interval_sec", &v) == 0) {
        r->snapshot_interval_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "runtime_tick_interval_sec", &v) == 0) {
        r->runtime_tick_interval_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "ready_grace_sec", &v) == 0) {
        r->ready_grace_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "valve_action_timeout_sec", &v) == 0) {
        r->valve_action_timeout_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    if (proto_json_get_u32(json, "vfd_action_timeout_sec", &v) == 0) {
        r->vfd_action_timeout_sec = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
    uint32_t wf = 0U;
    if (proto_json_get_u32(json, "workflow_enabled", &wf) == 0) {
        r->workflow_enabled = (uint8_t)(wf != 0U ? 1U : 0U);
    }
    if (r->link_ping_interval_sec > 0U) {
        if (r->vitals_interval_sec == 0U) {
            r->vitals_interval_sec = 900U;
        }
        if (r->vitals_csq_delta == 0U) {
            r->vitals_csq_delta = 5U;
        }
        if (r->vitals_soc_delta == 0U) {
            r->vitals_soc_delta = 5U;
        }
    } else if (r->heartbeat_interval_sec == 0U) {
        r->heartbeat_interval_sec = 60U;
    }
    if (r->snapshot_interval_sec == 0U) {
        r->snapshot_interval_sec = 300U;
    }
    return 0;
}

static int parse_runtime_rules_nested(const char *json, runtime_rules_t *r)
{
    char buf[512];
    if (extract_object_for_key(json, "runtime_rules", buf, sizeof(buf)) == 0) {
        return parse_runtime_rules(buf, r);
    }
    return parse_runtime_rules(json, r);
}

static int parse_bindings(const char *json, device_config_t *cfg)
{
    cfg->channel_binding_count = 0U;
    const char *p = strstr(json, "\"channel_bindings\"");
    if (!p) {
        return 0;
    }
    p = strchr(p, '[');
    if (!p) {
        return -1;
    }
    p++;
    char objbuf[512];
    while (cfg->channel_binding_count < MODEL_MAX_CHANNEL_BINDINGS) {
        p = skip_ws(p);
        if (*p == ']' || *p == '\0') {
            break;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (copy_json_object(p, objbuf, sizeof(objbuf), &p) != 0) {
            return -2;
        }
        channel_binding_t *b = &cfg->channel_bindings[cfg->channel_binding_count];
        memset(b, 0, sizeof(*b));
        (void)proto_json_get_string(objbuf, "channel_code", b->channel_code, sizeof(b->channel_code));
        (void)proto_json_get_string(objbuf, "module_code", b->module_code, sizeof(b->module_code));
        (void)proto_json_get_string(objbuf, "channel_role", b->channel_role, sizeof(b->channel_role));
        (void)proto_json_get_string(objbuf, "io_kind", b->io_kind, sizeof(b->io_kind));
        (void)proto_json_get_string(objbuf, "resource_ref", b->resource_ref, sizeof(b->resource_ref));
        uint32_t en = 1U;
        (void)proto_json_get_u32(objbuf, "enabled", &en);
        b->enabled = (uint8_t)(en != 0U ? 1U : 0U);
        cfg->channel_binding_count++;
    }
    return 0;
}

static int validate_config(const device_config_t *cfg)
{
    for (uint16_t i = 0U; i < cfg->channel_binding_count; i++) {
        for (uint16_t j = (uint16_t)(i + 1U); j < cfg->channel_binding_count; j++) {
            if (cfg->channel_bindings[i].channel_code[0] != '\0' &&
                strcmp(cfg->channel_bindings[i].channel_code, cfg->channel_bindings[j].channel_code) == 0) {
                return -1;
            }
            if (cfg->channel_bindings[i].resource_ref[0] != '\0' &&
                strcmp(cfg->channel_bindings[i].resource_ref, cfg->channel_bindings[j].resource_ref) == 0) {
                return -2;
            }
        }
    }
    return 0;
}

int proto_sync_config_apply(const char *json, size_t json_len, char *ack_json, size_t ack_cap)
{
    (void)json_len;
    if (!json) {
        return -1;
    }
    device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    device_config_t prev;
    int             prev_ok = storage_config_load(&prev);
    if (proto_json_get_u32(json, "config_version", &cfg.config_version) != 0) {
        return -2;
    }
    if (prev_ok == 0) {
        (void)memcpy(cfg.platform_tcp_host, prev.platform_tcp_host, sizeof(cfg.platform_tcp_host));
        cfg.platform_tcp_port = prev.platform_tcp_port;
    }
    {
        char     phost[MODEL_PLATFORM_HOST_MAX];
        uint32_t pport = 0U;
        if (proto_json_get_string(json, "platform_tcp_host", phost, sizeof(phost)) == 0) {
            (void)memcpy(cfg.platform_tcp_host, phost, sizeof(cfg.platform_tcp_host));
        }
        if (proto_json_get_u32(json, "platform_tcp_port", &pport) == 0 && pport <= 65535U) {
            cfg.platform_tcp_port = (uint16_t)pport;
        }
    }
    if (parse_features_nested(json, &cfg.feature_modules) != 0) {
        return -3;
    }
    if (parse_runtime_rules_nested(json, &cfg.runtime_rules) != 0) {
        return -4;
    }
    if (parse_bindings(json, &cfg) != 0) {
        return -5;
    }
    if (validate_config(&cfg) != 0) {
        return -6;
    }
    if (storage_config_stage_inactive(&cfg) != 0) {
        return -7;
    }
    if (storage_config_commit_swap(cfg.config_version) != 0) {
        return -8;
    }
    if (ack_json && ack_cap > 0U) {
        (void)snprintf(ack_json, ack_cap, "\"config_version\":%lu", (unsigned long)cfg.config_version);
    }
    return 0;
}
