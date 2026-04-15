#include "proto_state_snapshot.h"

#include "config_store.h"
#include "module_meter.h"
#include "module_relay_output.h"
#include "module_single_valve.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "runtime_state.h"

#include <stddef.h>
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

static uint8_t meter_feature_enabled(const device_config_t *cfg)
{
    return (uint8_t)(cfg != NULL && cfg->feature_modules.electric_meter_modbus != 0U);
}

static const char *meter_protocol_value(const device_config_t *cfg)
{
    if (cfg == NULL || cfg->feature_modules.electric_meter_modbus == 0U) {
        return NULL;
    }
    return module_meter_source_name();
}

static const char *reported_module_code(const char *module_code)
{
    if (module_code == NULL) {
        return NULL;
    }
    if (strcmp(module_code, "pump_direct_control") == 0 ||
        strcmp(module_code, "breaker_control") == 0) {
        return "breaker_control";
    }
    return module_code;
}

static uint8_t channel_should_emit(const device_config_t *cfg, const channel_binding_t *binding)
{
    const char *module_code;

    if (cfg == NULL || binding == NULL || binding->enabled == 0U) {
        return 0U;
    }
    module_code = reported_module_code(binding->module_code);
    if (module_code == NULL) {
        return 0U;
    }
    if (strcmp(module_code, "breaker_control") == 0) {
        return cfg->feature_modules.breaker_control != 0U ? 1U : 0U;
    }
    if (strcmp(module_code, "electric_meter_modbus") == 0) {
        return cfg->feature_modules.electric_meter_modbus != 0U ? 1U : 0U;
    }
    if (strcmp(module_code, "pump_vfd_control") == 0) {
        return cfg->feature_modules.pump_vfd_control != 0U ? 1U : 0U;
    }
    if (strcmp(module_code, "card_auth_reader") == 0) {
        return cfg->feature_modules.card_auth_reader != 0U ? 1U : 0U;
    }
    if (strcmp(module_code, "dual_valve_control") == 0) {
        return cfg->feature_modules.dual_valve_control != 0U ? 1U : 0U;
    }
    if (strcmp(module_code, "relay_output_control") == 0) {
        return cfg->feature_modules.relay_output_control != 0U ? 1U : 0U;
    }
    if (strcmp(module_code, "rs485_sensor_gateway") == 0) {
        return cfg->feature_modules.rs485_sensor_gateway != 0U ? 1U : 0U;
    }
    return 0U;
}

static uint8_t snapshot_metric_valid(float value, float min_allowed, float max_allowed)
{
    return (value >= min_allowed && value <= max_allowed) ? 1U : 0U;
}

static int append_channel_state_or_value(json_buf_t *jb, const channel_binding_t *binding, const runtime_state_t *rs)
{
    module_valve_state_t valve_state;
    module_relay_output_state_t relay_state;
    const char *valve_state_name = NULL;
    const char *module_code;

    if (jb == NULL || binding == NULL || rs == NULL) {
        return -1;
    }

    module_code = reported_module_code(binding->module_code);
    if (module_code == NULL) {
        return 0;
    }

    if (strcmp(module_code, "breaker_control") == 0 ||
        strcmp(module_code, "pump_vfd_control") == 0) {
        return json_buf_append_fmt(jb, ",\"st\":\"%s\"", runtime_state_pump_name(rs->pump_state));
    }
    if (strcmp(module_code, "card_auth_reader") == 0) {
        return json_buf_append_fmt(jb, ",\"st\":\"%s\"", binding->enabled != 0U ? "enabled" : "disabled");
    }
    if (strcmp(module_code, "electric_meter_modbus") == 0) {
        return 0;
    }
    if (strcmp(module_code, "dual_valve_control") == 0 &&
        module_single_valve_query_state_by_target(binding->channel_code, &valve_state) == 0) {
        switch (valve_state) {
        case VALVE_OPENING:
            valve_state_name = "opening";
            break;
        case VALVE_OPEN:
            valve_state_name = "open";
            break;
        case VALVE_CLOSING:
            valve_state_name = "closing";
            break;
        case VALVE_ACTION_TIMEOUT:
            valve_state_name = "timeout";
            break;
        case VALVE_CLOSED:
        default:
            valve_state_name = "closed";
            break;
        }
        return json_buf_append_fmt(jb, ",\"st\":\"%s\"", valve_state_name);
    }
    if (strcmp(module_code, "relay_output_control") == 0 &&
        module_relay_output_query_state_by_target(binding->channel_code, &relay_state) == 0) {
        return json_buf_append_fmt(jb, ",\"st\":\"%s\"",
                                   relay_state == RELAY_OUTPUT_ON ? "on" : "off");
    }
    return 0;
}

static int append_channels(json_buf_t *jb, const device_config_t *cfg, const runtime_state_t *rs)
{
    uint16_t i;
    uint8_t emitted = 0U;

    if (jb == NULL || rs == NULL) {
        return -1;
    }
    if (json_buf_append(jb, "\"ch\":[") != 0) {
        return -1;
    }
    if (cfg == NULL) {
        return json_buf_append(jb, "]");
    }
    for (i = 0U; i < cfg->channel_binding_count; i++) {
        const channel_binding_t *binding = &cfg->channel_bindings[i];
        const char *module_code = reported_module_code(binding->module_code);

        if (channel_should_emit(cfg, binding) == 0U || module_code == NULL) {
            continue;
        }
        if (emitted != 0U && json_buf_append(jb, ",") != 0) {
            return -1;
        }
        emitted = 1U;
        if (json_buf_append(jb, "{") != 0 ||
            json_buf_append_fmt(jb,
                                "\"mc\":\"%s\",\"cc\":\"%s\",\"ir\":\"%s\",\"en\":%u",
                                proto_map_module_short(module_code),
                                binding->channel_code,
                                binding->io_kind,
                                binding->enabled != 0U ? 1U : 0U) != 0 ||
            append_channel_state_or_value(jb, binding, rs) != 0 ||
            json_buf_append(jb, "}") != 0) {
            return -1;
        }
    }
    return json_buf_append(jb, "]");
}

int proto_state_snapshot_build(char *buf, size_t cap)
{
    json_buf_t jb;
    const runtime_state_t *rs = runtime_state_get();
    const device_config_t *cfg = config_store_active();
    uint8_t meter_enabled;
    uint8_t meter_valid;
    uint8_t first = 1U;

    if (buf == NULL || cap < 384U || rs == NULL) {
        return -1;
    }

    meter_enabled = meter_feature_enabled(cfg);
    meter_valid = (meter_enabled != 0U && rs->meter_last.valid != 0U) ? 1U : 0U;

    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_STATE_SNAPSHOT, 0U, NULL, NULL) != 0) {
        return -2;
    }
    if (append_optional_separator(&jb, &first) != 0 ||
        append_string_field(&jb, "wf",
                            proto_map_workflow_short_from_runtime(runtime_state_workflow_name(rs->workflow_state),
                                                                  rs->ready ? 1U : 0U)) != 0 ||
        append_optional_separator(&jb, &first) != 0 ||
        append_u32_field(&jb, "rt", rs->runtime_sec) != 0) {
        return -3;
    }
    if (meter_enabled != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_u32_field(&jb, "me", rs->meter_epoch) != 0) {
            return -3;
        }
    }
    if (snapshot_metric_valid(rs->total_m3, 0.0f, 10000000.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "fq", rs->total_m3, 2U) != 0) {
            return -3;
        }
    }
    if (meter_valid != 0U && snapshot_metric_valid(rs->voltage_v, 0.0f, 1000.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "vv", rs->voltage_v, 1U) != 0) {
            return -3;
        }
    }
    if (meter_valid != 0U && snapshot_metric_valid(rs->current_a, 0.0f, 1000.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "ia", rs->current_a, 1U) != 0) {
            return -3;
        }
    }
    if (meter_valid != 0U && snapshot_metric_valid(rs->power_kw, 0.0f, 500.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "pw", rs->power_kw, 2U) != 0) {
            return -3;
        }
    }
    if (meter_valid != 0U && snapshot_metric_valid(rs->energy_kwh, 0.0f, 10000000.0f) != 0U) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_fixed_field(&jb, "ek", rs->energy_kwh, 2U) != 0) {
            return -3;
        }
    }
    if (meter_protocol_value(cfg) != NULL) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_string_field(&jb, "mp", meter_protocol_value(cfg)) != 0) {
            return -3;
        }
    }
    if (breaker_state_value(rs) != NULL) {
        if (append_optional_separator(&jb, &first) != 0 ||
            append_string_field(&jb, "brs", breaker_state_value(rs)) != 0) {
            return -3;
        }
    }
    if (append_optional_separator(&jb, &first) != 0 ||
        append_channels(&jb, cfg, rs) != 0) {
        return -3;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -4;
    }
    return (int)jb.len;
}
