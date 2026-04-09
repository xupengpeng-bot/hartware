#include "proto_state_snapshot.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "common_status.h"
#include "workflow_engine.h"
#include "workflow_voice.h"
#include "module_pressure.h"
#include "module_flow.h"
#include "module_pump_vfd.h"

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

int proto_state_snapshot_build(char *buf, size_t cap)
{
    if (!buf || cap < 1024U) {
        return -1;
    }
    const common_status_t *cs = common_status_get();
    workflow_state_t       wf = workflow_engine_get_state();
    device_runtime_t      *rt = workflow_engine_runtime();
    workflow_voice_state_t voice_state;
    uint32_t               guard_remaining_ms = workflow_engine_stop_guard_remaining_ms();
    const char            *active_session_id = (rt && rt->active_session.session_id[0] != '\0')
                                                 ? rt->active_session.session_id
                                                 : "";
    const char            *last_session_id = (rt && rt->recovery.last_session_id[0] != '\0')
                                               ? rt->recovery.last_session_id
                                               : "";
    const char            *last_recovery_hint = (rt && rt->recovery.last_recovery_hint[0] != '\0')
                                                  ? rt->recovery.last_recovery_hint
                                                  : "";

    workflow_voice_get_state(&voice_state);

    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_STATE_SNAPSHOT, 0U, NULL, NULL) != 0) {
        return -2;
    }
    char tmp[768];
    (void)snprintf(tmp, sizeof(tmp),
                   "\"common_status\":{"
                   "\"online\":%s,\"tcp_connected\":%s,\"ready\":%s,"
                   "\"config_version\":%lu,\"signal_csq\":%d,\"battery_soc\":%u"
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
                   cs->online ? "true" : "false", cs->tcp_connected ? "true" : "false",
                   cs->ready ? "true" : "false",
                   (unsigned long)cs->config_version, (int)cs->signal_csq,
                   (unsigned)cs->battery_soc,
                   cs->registered_once ? "true" : "false",
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
    if (json_buf_append(&jb, tmp) != 0) {
        return -2;
    }
    (void)snprintf(tmp, sizeof(tmp),
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
    if (json_buf_append(&jb, tmp) != 0) {
        return -2;
    }
    float pmpa = 0.0f;
    float flow = 0.0f;
    (void)module_pressure_get_mpa(&pmpa);
    (void)module_flow_get_instant(&flow);
    uint8_t pump_st = 0U;
    (void)module_pump_vfd_query_state_u8(&pump_st);
    (void)snprintf(tmp, sizeof(tmp),
                   ",\"channel_values\":["
                   "{\"module_code\":\"pressure_acquisition\",\"channel_code\":\"pressure_1\",\"metric_code\":\"pressure_mpa\",\"value\":%.4f,\"unit\":\"MPa\",\"quality\":\"good\"},"
                   "{\"module_code\":\"flow_acquisition\",\"channel_code\":\"flow_1\",\"metric_code\":\"flow_m3h\",\"value\":%.4f,\"unit\":\"m3/h\",\"quality\":\"good\"},"
                   "{\"module_code\":\"pump_vfd_control\",\"channel_code\":\"pump_state\",\"metric_code\":\"pump_state\",\"value\":%u,\"unit\":null,\"quality\":\"good\"}"
                   "]",
                   (double)pmpa, (double)flow, (unsigned)pump_st);
    if (json_buf_append(&jb, tmp) != 0) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}
