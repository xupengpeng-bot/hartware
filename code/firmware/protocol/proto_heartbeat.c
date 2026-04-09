#include "proto_heartbeat.h"
#include "proto_json_builder.h"
#include "proto_envelope.h"
#include "common_status.h"
#include "workflow_engine.h"
#include "workflow_voice.h"
#include "cJSON.h"

#include <stdio.h>

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

static cJSON *build_controller_state_json(const common_status_t *st)
{
    device_runtime_t *rt = workflow_engine_runtime();
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    if (cJSON_AddBoolToObject(obj, "registered_once", st->registered_once) == NULL ||
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

static cJSON *build_voice_state_json(void)
{
    workflow_voice_state_t state;
    cJSON *obj = cJSON_CreateObject();
    workflow_voice_get_state(&state);
    if (obj == NULL) {
        return NULL;
    }
    if (cJSON_AddBoolToObject(obj, "enabled", state.enabled) == NULL ||
        cJSON_AddBoolToObject(obj, "supported", state.supported) == NULL ||
        cJSON_AddBoolToObject(obj, "busy", state.busy) == NULL ||
        cJSON_AddNumberToObject(obj, "queue_depth", (double)state.queue_depth) == NULL ||
        cJSON_AddStringToObject(obj, "last_prompt", state.last_prompt) == NULL ||
        cJSON_AddStringToObject(obj, "last_source", state.last_source) == NULL ||
        cJSON_AddNumberToObject(obj, "last_prompt_at_ms", (double)state.last_prompt_at_ms) == NULL) {
        cJSON_Delete(obj);
        return NULL;
    }
    return obj;
}

static int heartbeat_build_common_payload(char *buf, size_t cap, uint32_t seq, const char *kind, uint32_t uptime_sec, int vitals)
{
    const common_status_t *st = common_status_get();
    cJSON *payload = cJSON_CreateObject();
    cJSON *common_status = cJSON_CreateObject();
    cJSON *controller_state = build_controller_state_json(st);
    cJSON *voice_state = build_voice_state_json();
    int rc;

    if (payload == NULL || common_status == NULL || controller_state == NULL || voice_state == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice_state);
        return -2;
    }

    if (cJSON_AddStringToObject(payload, "heartbeat_kind", kind) == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice_state);
        return -2;
    }
    if (!vitals) {
        if (cJSON_AddNumberToObject(payload, "uptime_sec", (double)uptime_sec) == NULL) {
            cJSON_Delete(payload);
            cJSON_Delete(common_status);
            cJSON_Delete(controller_state);
            cJSON_Delete(voice_state);
            return -2;
        }
    }

    if ((vitals &&
         (cJSON_AddNumberToObject(common_status, "signal_csq", (double)st->signal_csq) == NULL ||
          cJSON_AddNumberToObject(common_status, "signal_rsrp", (double)st->rsrp_dbm) == NULL ||
          cJSON_AddNumberToObject(common_status, "signal_rsrq", (double)st->rsrq_db) == NULL ||
          cJSON_AddNumberToObject(common_status, "battery_soc", (double)st->battery_soc) == NULL ||
          cJSON_AddNumberToObject(common_status, "battery_voltage", (double)st->battery_voltage_v) == NULL ||
          cJSON_AddNumberToObject(common_status, "solar_voltage", (double)st->solar_voltage_v) == NULL ||
          cJSON_AddNumberToObject(common_status, "power_mode", (double)st->power_mode) == NULL)) ||
        cJSON_AddBoolToObject(common_status, "online", st->online) == NULL ||
        cJSON_AddBoolToObject(common_status, "tcp_connected", st->tcp_connected) == NULL ||
        cJSON_AddBoolToObject(common_status, "ready", st->ready) == NULL ||
        cJSON_AddNumberToObject(common_status, "config_version", (double)st->config_version) == NULL) {
        cJSON_Delete(payload);
        cJSON_Delete(common_status);
        cJSON_Delete(controller_state);
        cJSON_Delete(voice_state);
        return -2;
    }

    cJSON_AddItemToObject(payload, "common_status", common_status);
    cJSON_AddItemToObject(payload, "controller_state", controller_state);
    cJSON_AddItemToObject(payload, "voice_state", voice_state);

    rc = proto_json_build_message(buf, cap, PROTO_MSG_HEARTBEAT, seq, NULL, NULL, payload);
    return rc < 0 ? -2 : rc;
}

int proto_heartbeat_build_ping(char *buf, size_t cap, uint32_t seq, uint32_t uptime_sec)
{
    if (buf == NULL || cap < 640U) {
        return -1;
    }
    return heartbeat_build_common_payload(buf, cap, seq, "ping", uptime_sec, 0);
}

int proto_heartbeat_build_vitals(char *buf, size_t cap)
{
    if (buf == NULL || cap < 896U) {
        return -1;
    }
    return heartbeat_build_common_payload(buf, cap, 0U, "vitals", 0U, 1);
}

int proto_heartbeat_build(char *buf, size_t cap)
{
    return proto_heartbeat_build_vitals(buf, cap);
}
