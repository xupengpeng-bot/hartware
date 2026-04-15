#include "workflow_control.h"
#include "workflow_session.h"
#include "module_registry.h"
#include "workflow_voice.h"

#include <string.h>

static void workflow_control_start_primary_actuator(void)
{
    const module_ops_t *pump = module_registry_get("pump_vfd_control");

    if (pump && pump->execute_action) {
        (void)pump->execute_action("start_vfd", NULL, NULL);
    }
}

static void workflow_control_stop_primary_actuator(void)
{
    const module_ops_t *pump = module_registry_get("pump_vfd_control");

    if (pump && pump->execute_action) {
        (void)pump->execute_action("stop_vfd", NULL, NULL);
    }
}

const char *workflow_control_error_message(int code)
{
    switch (code) {
    case WORKFLOW_REQ_ERR_INVALID_ARG:
        return "invalid workflow state";
    case WORKFLOW_REQ_ERR_NOT_READY:
        return "controller not ready";
    case WORKFLOW_REQ_ERR_BUSY:
        return "another session is active";
    case WORKFLOW_REQ_ERR_DEBOUNCE:
        return "action debounced";
    case WORKFLOW_REQ_ERR_STOP_GUARD:
        return "stop settlement guard active";
    case WORKFLOW_REQ_ERR_SESSION_MISMATCH:
        return "session mismatch";
    default:
        return "workflow rejected";
    }
}

const char *workflow_control_state_label(workflow_state_t state)
{
    switch (state) {
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

int workflow_control_start_session(const char *preferred_session_ref,
                                   char       *resolved_session_ref,
                                   size_t      resolved_session_ref_cap,
                                   bool       *idempotent)
{
    workflow_session_context_t session;
    int                        result;

    memset(&session, 0, sizeof(session));
    if (preferred_session_ref && preferred_session_ref[0] != '\0') {
        (void)strncpy(session.session_id, preferred_session_ref, sizeof(session.session_id) - 1U);
    } else {
        workflow_session_generate_id(session.session_id, sizeof(session.session_id));
    }

    result = workflow_engine_request_start_session(&session);
    if (resolved_session_ref && resolved_session_ref_cap > 0U) {
        (void)strncpy(resolved_session_ref, session.session_id, resolved_session_ref_cap - 1U);
        resolved_session_ref[resolved_session_ref_cap - 1U] = '\0';
    }
    if (idempotent) {
        *idempotent = (bool)(result == WORKFLOW_REQ_IDEMPOTENT);
    }
    if (result == WORKFLOW_REQ_OK) {
        workflow_control_start_primary_actuator();
        workflow_voice_prompt_once("irrigation_started", "workflow_start", 1000U);
    }

    return result;
}

int workflow_control_stop_session(const char *session_ref, bool *idempotent)
{
    int result = workflow_engine_request_stop_session(session_ref);

    if (idempotent) {
        *idempotent = (bool)(result == WORKFLOW_REQ_IDEMPOTENT);
    }
    if (result == WORKFLOW_REQ_OK) {
        workflow_control_stop_primary_actuator();
        workflow_voice_prompt_once("irrigation_finished", "workflow_stop", 1000U);
    }

    return result;
}
