#include "proto_heartbeat.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "common_status.h"
#include "workflow_engine.h"
#include "workflow_voice.h"
#include <stdio.h>

static const char *workflow_state_str(workflow_state_t st)
{
    switch (st) {
    case WF_BOOTING:
        return "booting";
    case WF_ONLINE_NOT_READY:
        return "online_not_ready";
    case WF_READY_IDLE:
        return "ready_idle";
    case WF_STARTING:
        return "starting";
    case WF_RUNNING:
        return "running";
    case WF_PAUSING:
        return "pausing";
    case WF_PAUSED:
        return "paused";
    case WF_RESUMING:
        return "resuming";
    case WF_STOPPING:
        return "stopping";
    case WF_STOPPED:
        return "stopped";
    case WF_ERROR_STOP:
        return "error_stop";
    default:
        return "unknown";
    }
}

static int heartbeat_append_controller_state(json_buf_t *jb, const common_status_t *st)
{
    device_runtime_t *rt = workflow_engine_runtime();
    workflow_state_t  wf = workflow_engine_get_state();
    uint32_t          guard_remaining_ms = workflow_engine_stop_guard_remaining_ms();
    const char       *active_session_id = (rt && rt->active_session.session_id[0] != '\0')
                                            ? rt->active_session.session_id
                                            : "";
    const char       *last_session_id = (rt && rt->recovery.last_session_id[0] != '\0')
                                          ? rt->recovery.last_session_id
                                          : "";
    const char       *last_recovery_hint = (rt && rt->recovery.last_recovery_hint[0] != '\0')
                                             ? rt->recovery.last_recovery_hint
                                             : "";
    char              tail[512];

    (void)snprintf(tail, sizeof(tail),
                   "},\"controller_state\":{"
                   "\"registered_once\":%s,"
                   "\"workflow_state\":\"%s\","
                   "\"active_session_id\":\"%s\","
                   "\"active_session_started_at_utc\":%lu,"
                   "\"recovery_pending\":%s,"
                   "\"settlement_pending\":%s,"
                   "\"stop_guard_remaining_ms\":%lu,"
                   "\"last_stop_reason_code\":%lu,"
                   "\"last_stop_at_utc\":%lu,"
                   "\"last_session_id\":\"%s\","
                   "\"last_session_started_at_utc\":%lu,"
                   "\"last_recovery_hint\":\"%s\""
                   "}",
                   st->registered_once ? "true" : "false",
                   workflow_state_str(wf),
                   active_session_id,
                   (unsigned long)(rt ? rt->active_session.started_at_utc : 0U),
                   (rt && rt->recovery.recovery_pending) ? "true" : "false",
                   (rt && rt->recovery.settlement_pending) ? "true" : "false",
                   (unsigned long)guard_remaining_ms,
                   (unsigned long)(rt ? rt->recovery.last_stop_reason_code : 0U),
                   (unsigned long)(rt ? rt->recovery.last_stop_at_utc : 0U),
                   last_session_id,
                   (unsigned long)(rt ? rt->recovery.last_session_started_at_utc : 0U),
                   last_recovery_hint);
    return json_buf_append(jb, tail);
}

static int heartbeat_append_voice_state(json_buf_t *jb)
{
    workflow_voice_state_t voice_state;
    char                   tail[320];

    workflow_voice_get_state(&voice_state);
    (void)snprintf(tail, sizeof(tail),
                   ",\"voice_state\":{"
                   "\"enabled\":%s,"
                   "\"supported\":%s,"
                   "\"busy\":%s,"
                   "\"queue_depth\":%lu,"
                   "\"last_prompt\":\"%s\","
                   "\"last_source\":\"%s\","
                   "\"last_prompt_at_ms\":%lu"
                   "}",
                   voice_state.enabled ? "true" : "false",
                   voice_state.supported ? "true" : "false",
                   voice_state.busy ? "true" : "false",
                   (unsigned long)voice_state.queue_depth,
                   voice_state.last_prompt,
                   voice_state.last_source,
                   (unsigned long)voice_state.last_prompt_at_ms);
    return json_buf_append(jb, tail);
}

int proto_heartbeat_build_ping(char *buf, size_t cap, uint32_t seq, uint32_t uptime_sec)
{
    if (!buf || cap < 640U) {
        return -1;
    }
    const common_status_t       *st = common_status_get();

    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_HEARTBEAT, seq, NULL, NULL) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"heartbeat_kind\":\"ping\",\"uptime_sec\":") != 0) {
        return -2;
    }
    char tail[256];
    (void)snprintf(tail, sizeof(tail),
                    "%lu,\"common_status\":{"
                    "\"online\":%s,\"tcp_connected\":%s,\"ready\":%s,"
                    "\"config_version\":%lu"
                    ,
                    (unsigned long)uptime_sec,
                    st->online ? "true" : "false",
                    st->tcp_connected ? "true" : "false",
                    st->ready ? "true" : "false",
                    (unsigned long)st->config_version);
    if (json_buf_append(&jb, tail) != 0) {
        return -2;
    }
    if (heartbeat_append_controller_state(&jb, st) != 0) {
        return -2;
    }
    if (heartbeat_append_voice_state(&jb) != 0) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}

int proto_heartbeat_build_vitals(char *buf, size_t cap)
{
    if (!buf || cap < 896U) {
        return -1;
    }
    const common_status_t       *st = common_status_get();

    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_HEARTBEAT, 0U, NULL, NULL) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"heartbeat_kind\":\"vitals\",\"common_status\":{") != 0) {
        return -2;
    }

    char tail[512];
    (void)snprintf(tail, sizeof(tail),
                    "\"signal_csq\":%d,"
                    "\"signal_rsrp\":%d,"
                    "\"signal_rsrq\":%d,"
                    "\"battery_soc\":%u,"
                    "\"battery_voltage\":%.2f,"
                    "\"solar_voltage\":%.2f,"
                    "\"power_mode\":%u,"
                    "\"online\":%s,"
                    "\"tcp_connected\":%s,"
                    "\"ready\":%s,"
                    "\"config_version\":%lu"
                    ,
                    (int)st->signal_csq,
                    (int)st->rsrp_dbm,
                    (int)st->rsrq_db,
                    (unsigned)st->battery_soc,
                    (double)st->battery_voltage_v,
                    (double)st->solar_voltage_v,
                    (unsigned)st->power_mode,
                    st->online ? "true" : "false",
                    st->tcp_connected ? "true" : "false",
                    st->ready ? "true" : "false",
                    (unsigned long)st->config_version);
    if (json_buf_append(&jb, tail) != 0) {
        return -2;
    }
    if (heartbeat_append_controller_state(&jb, st) != 0) {
        return -2;
    }
    if (heartbeat_append_voice_state(&jb) != 0) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}

int proto_heartbeat_build(char *buf, size_t cap)
{
    return proto_heartbeat_build_vitals(buf, cap);
}
