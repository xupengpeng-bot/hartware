#include "proto_query.h"
#include "proto_codec_json.h"
#include "proto_json_builder.h"
#include "proto_envelope.h"
#include "common_status.h"
#include "workflow_engine.h"
#include "workflow_card_reader.h"
#include "workflow_local_access.h"
#include "workflow_voice.h"
#include "module_registry.h"
#include "module_pressure.h"
#include "module_flow.h"
#include "module_meter.h"
#include "module_soil_moisture.h"
#include "module_soil_temperature.h"
#include "proto_ota.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>

static const char *ota_state_str(ota_state_t st)
{
    switch (st) {
    case OTA_STATE_IDLE: return "IDLE";
    case OTA_STATE_PRECHECKING: return "PRECHECKING";
    case OTA_STATE_PRECHECK_FAILED: return "PRECHECK_FAILED";
    case OTA_STATE_READY_TO_DOWNLOAD: return "READY_TO_DOWNLOAD";
    case OTA_STATE_DOWNLOADING: return "DOWNLOADING";
    case OTA_STATE_DOWNLOAD_FAILED: return "DOWNLOAD_FAILED";
    case OTA_STATE_DOWNLOADED: return "DOWNLOADED";
    case OTA_STATE_VERIFYING: return "VERIFYING";
    case OTA_STATE_VERIFY_FAILED: return "VERIFY_FAILED";
    case OTA_STATE_VERIFIED: return "VERIFIED";
    case OTA_STATE_WRITING: return "WRITING";
    case OTA_STATE_WRITE_FAILED: return "WRITE_FAILED";
    case OTA_STATE_READY_TO_SWITCH: return "READY_TO_SWITCH";
    case OTA_STATE_SWITCHING: return "SWITCHING";
    case OTA_STATE_UPGRADED: return "UPGRADED";
    case OTA_STATE_UPGRADE_FAILED: return "UPGRADE_FAILED";
    case OTA_STATE_ROLLING_BACK: return "ROLLING_BACK";
    case OTA_STATE_ROLLED_BACK: return "ROLLED_BACK";
    default: return "UNKNOWN";
    }
}

static const char *workflow_state_str(workflow_state_t st)
{
    switch (st) {
    case WF_BOOTING: return "booting";
    case WF_ONLINE_NOT_READY: return "online_not_ready";
    case WF_READY_IDLE: return "ready_idle";
    case WF_STARTING: return "starting";
    case WF_RUNNING: return "running";
    case WF_PAUSING: return "pausing";
    case WF_PAUSED: return "paused";
    case WF_RESUMING: return "resuming";
    case WF_STOPPING: return "stopping";
    case WF_STOPPED: return "stopped";
    case WF_ERROR_STOP: return "error_stop";
    default: return "unknown";
    }
}

static int add_channel_value(cJSON *arr, const char *module_code, const char *channel_code,
                             const char *metric_code, double value, const char *unit, const char *quality)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return -1;
    }
    if (cJSON_AddStringToObject(obj, "module_code", module_code) == NULL ||
        cJSON_AddStringToObject(obj, "channel_code", channel_code) == NULL ||
        cJSON_AddStringToObject(obj, "metric_code", metric_code) == NULL ||
        cJSON_AddNumberToObject(obj, "value", value) == NULL ||
        ((unit != NULL) ? (cJSON_AddStringToObject(obj, "unit", unit) != NULL) : (cJSON_AddNullToObject(obj, "unit") != NULL)) == 0 ||
        cJSON_AddStringToObject(obj, "quality", quality) == NULL) {
        cJSON_Delete(obj);
        return -1;
    }
    cJSON_AddItemToArray(arr, obj);
    return 0;
}

static int build_workflow_state_payload(cJSON *payload)
{
    workflow_state_t st = workflow_engine_get_state();
    device_runtime_t *rt = workflow_engine_runtime();
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return -1;
    }
    if (cJSON_AddStringToObject(obj, "workflow_state", workflow_state_str(st)) == NULL ||
        cJSON_AddStringToObject(obj, "active_session_id", (rt && rt->active_session.session_id[0] != '\0') ? rt->active_session.session_id : "") == NULL ||
        cJSON_AddNumberToObject(obj, "active_session_started_at_utc", (double)(rt ? rt->active_session.started_at_utc : 0U)) == NULL ||
        cJSON_AddBoolToObject(obj, "recovery_pending", (rt && rt->recovery.recovery_pending) ? 1 : 0) == NULL ||
        cJSON_AddBoolToObject(obj, "settlement_pending", (rt && rt->recovery.settlement_pending) ? 1 : 0) == NULL ||
        cJSON_AddNumberToObject(obj, "stop_guard_remaining_ms", (double)workflow_engine_stop_guard_remaining_ms()) == NULL ||
        cJSON_AddNumberToObject(obj, "last_stop_reason_code", (double)(rt ? rt->recovery.last_stop_reason_code : 0U)) == NULL ||
        cJSON_AddNumberToObject(obj, "last_stop_at_utc", (double)(rt ? rt->recovery.last_stop_at_utc : 0U)) == NULL ||
        cJSON_AddStringToObject(obj, "last_session_id", (rt && rt->recovery.last_session_id[0] != '\0') ? rt->recovery.last_session_id : "") == NULL ||
        cJSON_AddNumberToObject(obj, "last_session_started_at_utc", (double)(rt ? rt->recovery.last_session_started_at_utc : 0U)) == NULL ||
        cJSON_AddStringToObject(obj, "last_recovery_hint", (rt && rt->recovery.last_recovery_hint[0] != '\0') ? rt->recovery.last_recovery_hint : "") == NULL) {
        cJSON_Delete(obj);
        return -1;
    }
    cJSON_AddItemToObject(payload, "controller_state", obj);
    return 0;
}

int proto_query_handle(const char *json, size_t json_len, char *reply, size_t reply_cap)
{
    char corr[48];
    char session_ref[64];
    char scope[24];
    char qcode[48];
    cJSON *payload;
    int rc;
    (void)json_len;

    if (!json || !reply || reply_cap < 64U) {
        return -1;
    }
    corr[0] = '\0';
    session_ref[0] = '\0';
    (void)proto_json_get_string(json, "correlation_id", corr, sizeof(corr));
    (void)proto_json_get_string(json, "session_ref", session_ref, sizeof(session_ref));
    if (session_ref[0] == '\0') {
        (void)proto_json_get_string(json, "session_id", session_ref, sizeof(session_ref));
    }
    if (proto_json_get_string(json, "scope", scope, sizeof(scope)) != 0) {
        return -2;
    }
    if (proto_json_get_string(json, "query_code", qcode, sizeof(qcode)) != 0) {
        return -3;
    }

    payload = cJSON_CreateObject();
    if (payload == NULL) {
        return -4;
    }
    if (cJSON_AddStringToObject(payload, "scope", scope) == NULL ||
        cJSON_AddStringToObject(payload, "query_code", qcode) == NULL) {
        cJSON_Delete(payload);
        return -4;
    }

    if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_workflow_state") == 0) {
        if (build_workflow_state_payload(payload) != 0) {
            cJSON_Delete(payload);
            return -4;
        }
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_local_access_policy") == 0) {
        workflow_local_access_policy_t policy;
        cJSON *obj = cJSON_CreateObject();
        workflow_local_access_get_policy(&policy);
        if (obj == NULL ||
            cJSON_AddStringToObject(obj, "mode", "card_or_local_token") == NULL ||
            cJSON_AddNumberToObject(obj, "global_debounce_ms", (double)policy.global_debounce_ms) == NULL ||
            cJSON_AddNumberToObject(obj, "same_token_debounce_ms", (double)policy.same_token_debounce_ms) == NULL ||
            cJSON_AddNumberToObject(obj, "post_stop_drop_window_ms", (double)policy.post_stop_drop_window_ms) == NULL ||
            cJSON_AddBoolToObject(obj, "active_token_bound", workflow_local_access_has_active_token()) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "local_access", obj);
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_local_access_state") == 0) {
        workflow_local_access_state_t state;
        cJSON *obj = cJSON_CreateObject();
        workflow_local_access_get_state(&state);
        if (obj == NULL ||
            cJSON_AddStringToObject(obj, "last_outcome", workflow_local_access_outcome_label(state.last_outcome)) == NULL ||
            cJSON_AddStringToObject(obj, "last_reason", state.last_reason) == NULL ||
            cJSON_AddStringToObject(obj, "last_source", state.last_source) == NULL ||
            cJSON_AddStringToObject(obj, "last_session_id", state.last_session_id) == NULL ||
            cJSON_AddNumberToObject(obj, "last_seen_at_ms", (double)state.last_seen_at_ms) == NULL ||
            cJSON_AddNumberToObject(obj, "last_decision_at_ms", (double)state.last_decision_at_ms) == NULL ||
            cJSON_AddBoolToObject(obj, "last_idempotent", state.last_idempotent) == NULL ||
            cJSON_AddBoolToObject(obj, "active_token_bound", state.active_token_bound) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "local_access_state", obj);
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_card_reader_state") == 0) {
        workflow_card_reader_state_t state;
        cJSON *obj = cJSON_CreateObject();
        workflow_card_reader_get_state(&state);
        if (obj == NULL ||
            cJSON_AddStringToObject(obj, "mode", "platform_checkout_card_reader") == NULL ||
            cJSON_AddBoolToObject(obj, "enabled", state.enabled) == NULL ||
            cJSON_AddBoolToObject(obj, "supported", state.supported) == NULL ||
            cJSON_AddNumberToObject(obj, "uart_port", (double)state.uart_port) == NULL ||
            cJSON_AddNumberToObject(obj, "rx_buffered_bytes", (double)state.rx_buffered_bytes) == NULL ||
            cJSON_AddNumberToObject(obj, "frames_ok", (double)state.frames_ok) == NULL ||
            cJSON_AddNumberToObject(obj, "frames_invalid", (double)state.frames_invalid) == NULL ||
            cJSON_AddNumberToObject(obj, "reports_sent", (double)state.reports_sent) == NULL ||
            cJSON_AddNumberToObject(obj, "reports_failed", (double)state.reports_failed) == NULL ||
            cJSON_AddNumberToObject(obj, "debounce_dropped", (double)state.debounce_dropped) == NULL ||
            cJSON_AddNumberToObject(obj, "rejected_count", (double)state.rejected_count) == NULL ||
            cJSON_AddStringToObject(obj, "last_outcome", state.last_outcome) == NULL ||
            cJSON_AddStringToObject(obj, "last_reason", state.last_reason) == NULL ||
            cJSON_AddStringToObject(obj, "last_source", state.last_source) == NULL ||
            cJSON_AddStringToObject(obj, "last_token_suffix", state.last_token_suffix) == NULL ||
            cJSON_AddNumberToObject(obj, "last_swipe_at_ms", (double)state.last_swipe_at_ms) == NULL ||
            cJSON_AddNumberToObject(obj, "last_reported_at_ms", (double)state.last_reported_at_ms) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "card_reader_state", obj);
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_voice_state") == 0) {
        workflow_voice_state_t state;
        cJSON *obj = cJSON_CreateObject();
        workflow_voice_get_state(&state);
        if (obj == NULL ||
            cJSON_AddBoolToObject(obj, "enabled", state.enabled) == NULL ||
            cJSON_AddBoolToObject(obj, "supported", state.supported) == NULL ||
            cJSON_AddBoolToObject(obj, "busy", state.busy) == NULL ||
            cJSON_AddNumberToObject(obj, "queue_depth", (double)state.queue_depth) == NULL ||
            cJSON_AddStringToObject(obj, "last_prompt", state.last_prompt) == NULL ||
            cJSON_AddStringToObject(obj, "last_source", state.last_source) == NULL ||
            cJSON_AddNumberToObject(obj, "last_prompt_at_ms", (double)state.last_prompt_at_ms) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "voice_state", obj);
    } else if (strcmp(scope, "common") == 0 && strcmp(qcode, "query_upgrade_status") == 0) {
        ota_upgrade_status_t s;
        cJSON *obj = cJSON_CreateObject();
        memset(&s, 0, sizeof(s));
        (void)proto_ota_query_upgrade_status(&s);
        if (obj == NULL ||
            cJSON_AddStringToObject(obj, "ota_state", ota_state_str(s.ota_state)) == NULL ||
            cJSON_AddStringToObject(obj, "target_version", s.target_version) == NULL ||
            cJSON_AddStringToObject(obj, "current_version", s.current_version) == NULL ||
            cJSON_AddStringToObject(obj, "package_sha256", s.package_sha256_hex) == NULL ||
            cJSON_AddNumberToObject(obj, "download_progress_pct", (double)s.download_progress_pct) == NULL ||
            cJSON_AddNumberToObject(obj, "write_progress_pct", (double)s.write_progress_pct) == NULL ||
            cJSON_AddNumberToObject(obj, "last_result", (double)s.last_result) == NULL ||
            cJSON_AddNumberToObject(obj, "last_error_code", (double)s.last_error_code) == NULL ||
            cJSON_AddStringToObject(obj, "last_error_message", s.last_error_message) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "upgrade_status", obj);
    } else if (strcmp(scope, "common") == 0 && strcmp(qcode, "query_upgrade_capability") == 0) {
        ota_upgrade_capability_t c;
        cJSON *obj = cJSON_CreateObject();
        memset(&c, 0, sizeof(c));
        (void)proto_ota_query_upgrade_capability(&c);
        if (obj == NULL ||
            cJSON_AddBoolToObject(obj, "ota_supported", c.ota_supported) == NULL ||
            cJSON_AddBoolToObject(obj, "dual_bank", c.dual_bank) == NULL ||
            cJSON_AddStringToObject(obj, "package_formats", c.package_formats) == NULL ||
            cJSON_AddStringToObject(obj, "compression_formats", c.compression_formats) == NULL ||
            cJSON_AddNumberToObject(obj, "min_battery_soc_default", (double)c.min_battery_soc_default) == NULL ||
            cJSON_AddNumberToObject(obj, "min_signal_csq_default", (double)c.min_signal_csq_default) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "upgrade_capability", obj);
    } else if (strcmp(scope, "common") == 0 && strcmp(qcode, "query_common_status") == 0) {
        const common_status_t *cs = common_status_get();
        cJSON *obj = cJSON_CreateObject();
        if (obj == NULL ||
            cJSON_AddBoolToObject(obj, "online", cs->online) == NULL ||
            cJSON_AddBoolToObject(obj, "ready", cs->ready) == NULL ||
            cJSON_AddNumberToObject(obj, "signal_csq", (double)cs->signal_csq) == NULL ||
            cJSON_AddNumberToObject(obj, "battery_soc", (double)cs->battery_soc) == NULL ||
            cJSON_AddNumberToObject(obj, "config_version", (double)cs->config_version) == NULL) {
            cJSON_Delete(obj);
            cJSON_Delete(payload);
            return -4;
        }
        cJSON_AddItemToObject(payload, "common_status", obj);
    } else if (strcmp(scope, "module") == 0) {
        char mcode[48];
        if (proto_json_get_string(json, "module_code", mcode, sizeof(mcode)) != 0) {
            cJSON_AddStringToObject(payload, "error", "missing module_code");
        } else {
            const module_ops_t *m = module_registry_get(mcode);
            if (m == NULL) {
                cJSON_AddStringToObject(payload, "module_code", mcode);
                cJSON_AddStringToObject(payload, "error", "unknown module");
            } else {
                cJSON *arr = cJSON_CreateArray();
                if (arr == NULL) {
                    cJSON_Delete(payload);
                    return -4;
                }
                cJSON_AddStringToObject(payload, "module_code", mcode);
                if (strcmp(mcode, "pressure_acquisition") == 0) {
                    module_pressure_values_t v; memset(&v, 0, sizeof(v)); (void)m->query_values(&v);
                    if (add_channel_value(arr, mcode, "pressure_1", "pressure_mpa", (double)v.pressure_mpa, "MPa", v.quality != 0U ? "good" : "bad") != 0) { cJSON_Delete(arr); cJSON_Delete(payload); return -4; }
                } else if (strcmp(mcode, "flow_acquisition") == 0) {
                    module_flow_values_t v; memset(&v, 0, sizeof(v)); (void)m->query_values(&v);
                    if (add_channel_value(arr, mcode, "flow_1", "flow_m3h", (double)v.instant_m3h, "m3/h", "good") != 0 ||
                        add_channel_value(arr, mcode, "flow_total", "total_m3", (double)v.total_m3, "m3", "good") != 0) { cJSON_Delete(arr); cJSON_Delete(payload); return -4; }
                } else if (strcmp(mcode, "electric_meter_modbus") == 0) {
                    module_meter_values_t v; memset(&v, 0, sizeof(v)); (void)m->query_values(&v);
                    if (add_channel_value(arr, mcode, "meter_energy", "energy_kwh", (double)v.energy_kwh, "kWh", "good") != 0 ||
                        add_channel_value(arr, mcode, "meter_power", "power_kw", (double)v.power_kw, "kW", "good") != 0 ||
                        add_channel_value(arr, mcode, "meter_voltage", "voltage_v", (double)v.voltage_v, "V", "good") != 0 ||
                        add_channel_value(arr, mcode, "meter_current", "current_a", (double)v.current_a, "A", "good") != 0) { cJSON_Delete(arr); cJSON_Delete(payload); return -4; }
                } else if (strcmp(mcode, "soil_moisture_acquisition") == 0) {
                    module_soil_moisture_values_t v; memset(&v, 0, sizeof(v)); (void)m->query_values(&v);
                    if (add_channel_value(arr, mcode, "soil_moisture_1", "soil_moisture_vwc", (double)v.soil_moisture_vwc, NULL, "good") != 0) { cJSON_Delete(arr); cJSON_Delete(payload); return -4; }
                } else if (strcmp(mcode, "soil_temperature_acquisition") == 0) {
                    module_soil_temperature_values_t v; memset(&v, 0, sizeof(v)); (void)m->query_values(&v);
                    if (add_channel_value(arr, mcode, "soil_temperature_1", "soil_temperature_c", (double)v.soil_temperature_c, "C", "good") != 0) { cJSON_Delete(arr); cJSON_Delete(payload); return -4; }
                } else {
                    cJSON_Delete(arr);
                    cJSON_AddStringToObject(payload, "note", "no values serializer");
                    arr = NULL;
                }
                if (arr != NULL) {
                    cJSON_AddItemToObject(payload, "channel_values", arr);
                }
            }
        }
    } else {
        cJSON_AddStringToObject(payload, "error", "unsupported_query");
    }

    rc = proto_json_build_message(reply, reply_cap, PROTO_MSG_QUERY_RESULT, 0U,
                                  corr, session_ref[0] ? session_ref : NULL, payload);
    return rc < 0 ? -4 : rc;
}
