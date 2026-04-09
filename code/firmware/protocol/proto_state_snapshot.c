#include "proto_state_snapshot.h"
#include "proto_json_builder.h"
#include "proto_envelope.h"
#include "common_status.h"
#include "workflow_engine.h"
#include "workflow_voice.h"
#include "module_pressure.h"
#include "module_flow.h"
#include "module_pump_vfd.h"
#include "cJSON.h"

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

static cJSON *build_controller_state_json(const common_status_t *cs)
{
    device_runtime_t *rt = workflow_engine_runtime();
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    if (cJSON_AddBoolToObject(obj, "registered_once", cs->registered_once) == NULL ||
        cJSON_AddStringToObject(obj, "workflow_state", workflow_state_str(workflow_engine_get_state())) == NULL ||
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
        return NULL;
    }
    return obj;
}

int proto_state_snapshot_build(char *buf, size_t cap)
{
    const common_status_t *cs;
    workflow_voice_state_t voice_state;
    cJSON *payload = NULL;
    cJSON *common_status = NULL;
    cJSON *controller_state = NULL;
    cJSON *voice = NULL;
    cJSON *channel_values = NULL;
    cJSON *item = NULL;
    float pmpa = 0.0f;
    float flow = 0.0f;
    uint8_t pump_st = 0U;

    if (buf == NULL || cap < 1024U) {
        return -1;
    }
    cs = common_status_get();
    workflow_voice_get_state(&voice_state);
    (void)module_pressure_get_mpa(&pmpa);
    (void)module_flow_get_instant(&flow);
    (void)module_pump_vfd_query_state_u8(&pump_st);

    payload = cJSON_CreateObject();
    common_status = cJSON_CreateObject();
    controller_state = build_controller_state_json(cs);
    voice = cJSON_CreateObject();
    channel_values = cJSON_CreateArray();
    if (payload == NULL || common_status == NULL || controller_state == NULL || voice == NULL || channel_values == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice);
        cJSON_Delete(channel_values);
        return -2;
    }

    if (cJSON_AddBoolToObject(common_status, "online", cs->online) == NULL ||
        cJSON_AddBoolToObject(common_status, "tcp_connected", cs->tcp_connected) == NULL ||
        cJSON_AddBoolToObject(common_status, "ready", cs->ready) == NULL ||
        cJSON_AddNumberToObject(common_status, "config_version", (double)cs->config_version) == NULL ||
        cJSON_AddNumberToObject(common_status, "signal_csq", (double)cs->signal_csq) == NULL ||
        cJSON_AddNumberToObject(common_status, "battery_soc", (double)cs->battery_soc) == NULL ||
        cJSON_AddBoolToObject(voice, "enabled", voice_state.enabled) == NULL ||
        cJSON_AddBoolToObject(voice, "supported", voice_state.supported) == NULL ||
        cJSON_AddBoolToObject(voice, "busy", voice_state.busy) == NULL ||
        cJSON_AddNumberToObject(voice, "queue_depth", (double)voice_state.queue_depth) == NULL ||
        cJSON_AddStringToObject(voice, "last_prompt", voice_state.last_prompt) == NULL ||
        cJSON_AddStringToObject(voice, "last_source", voice_state.last_source) == NULL ||
        cJSON_AddNumberToObject(voice, "last_prompt_at_ms", (double)voice_state.last_prompt_at_ms) == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice);
        cJSON_Delete(channel_values);
        return -2;
    }

    item = cJSON_CreateObject();
    if (item == NULL ||
        cJSON_AddStringToObject(item, "module_code", "pressure_acquisition") == NULL ||
        cJSON_AddStringToObject(item, "channel_code", "pressure_1") == NULL ||
        cJSON_AddStringToObject(item, "metric_code", "pressure_mpa") == NULL ||
        cJSON_AddNumberToObject(item, "value", (double)pmpa) == NULL ||
        cJSON_AddStringToObject(item, "unit", "MPa") == NULL ||
        cJSON_AddStringToObject(item, "quality", "good") == NULL) {
        cJSON_Delete(item);
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice);
        cJSON_Delete(channel_values);
        return -2;
    }
    cJSON_AddItemToArray(channel_values, item);

    item = cJSON_CreateObject();
    if (item == NULL ||
        cJSON_AddStringToObject(item, "module_code", "flow_acquisition") == NULL ||
        cJSON_AddStringToObject(item, "channel_code", "flow_1") == NULL ||
        cJSON_AddStringToObject(item, "metric_code", "flow_m3h") == NULL ||
        cJSON_AddNumberToObject(item, "value", (double)flow) == NULL ||
        cJSON_AddStringToObject(item, "unit", "m3/h") == NULL ||
        cJSON_AddStringToObject(item, "quality", "good") == NULL) {
        cJSON_Delete(item);
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice);
        cJSON_Delete(channel_values);
        return -2;
    }
    cJSON_AddItemToArray(channel_values, item);

    item = cJSON_CreateObject();
    if (item == NULL ||
        cJSON_AddStringToObject(item, "module_code", "pump_vfd_control") == NULL ||
        cJSON_AddStringToObject(item, "channel_code", "pump_state") == NULL ||
        cJSON_AddStringToObject(item, "metric_code", "pump_state") == NULL ||
        cJSON_AddNumberToObject(item, "value", (double)pump_st) == NULL ||
        cJSON_AddNullToObject(item, "unit") == NULL ||
        cJSON_AddStringToObject(item, "quality", "good") == NULL) {
        cJSON_Delete(item);
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice);
        cJSON_Delete(channel_values);
        return -2;
    }
    cJSON_AddItemToArray(channel_values, item);

    cJSON_AddItemToObject(payload, "common_status", common_status);
    cJSON_AddItemToObject(payload, "controller_state", controller_state);
    cJSON_AddItemToObject(payload, "voice_state", voice);
    cJSON_AddItemToObject(payload, "channel_values", channel_values);

    return proto_json_build_message(buf, cap, PROTO_MSG_STATE_SNAPSHOT, 0U, NULL, NULL, payload);
}
