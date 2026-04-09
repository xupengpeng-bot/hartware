#include "workflow_local_access.h"
#include "workflow_control.h"
#include "workflow_engine.h"
#include "workflow_voice.h"
#include "common_status.h"
#include "app_context.h"
#include "net_connectivity.h"
#include "proto_event_report.h"

#include <stdio.h>
#include <string.h>

#define WORKFLOW_LOCAL_ACCESS_GLOBAL_DEBOUNCE_MS    250U
#define WORKFLOW_LOCAL_ACCESS_SAME_TOKEN_DEBOUNCE_MS 1500U
#define WORKFLOW_LOCAL_ACCESS_POST_STOP_DROP_MS     5000U

static char     s_last_token[WORKFLOW_LOCAL_ACCESS_TOKEN_LEN];
static uint32_t s_last_seen_at_ms;
static char     s_active_token[WORKFLOW_LOCAL_ACCESS_TOKEN_LEN];
static workflow_local_access_state_t s_state;

static void workflow_local_access_set_reason(char *reason, size_t reason_cap, const char *value)
{
    if (!reason || reason_cap == 0U) {
        return;
    }

    if (!value) {
        reason[0] = '\0';
        return;
    }

    (void)strncpy(reason, value, reason_cap - 1U);
    reason[reason_cap - 1U] = '\0';
}

static void workflow_local_access_copy_symbol(char *dst, size_t dst_cap, const char *src)
{
    size_t idx = 0U;

    if (!dst || dst_cap == 0U) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[idx] != '\0' && idx + 1U < dst_cap) {
        char ch = src[idx];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-') {
            dst[idx] = ch;
        } else {
            dst[idx] = '_';
        }
        idx++;
    }
    dst[idx] = '\0';
}

static int workflow_local_access_same_token(const char *left, const char *right)
{
    if (!left || !right || left[0] == '\0' || right[0] == '\0') {
        return 0;
    }
    return strcmp(left, right) == 0;
}

static void workflow_local_access_note_seen(const char *token, uint32_t now_ms)
{
    if (!token || token[0] == '\0') {
        return;
    }

    (void)strncpy(s_last_token, token, sizeof(s_last_token) - 1U);
    s_last_token[sizeof(s_last_token) - 1U] = '\0';
    s_last_seen_at_ms = now_ms;
    s_state.last_seen_at_ms = now_ms;
}

static workflow_local_access_decision_t workflow_local_access_map_workflow_error(int code)
{
    switch (code) {
    case WORKFLOW_REQ_ERR_DEBOUNCE:
        return WORKFLOW_LOCAL_ACCESS_REJECT_DEBOUNCE;
    case WORKFLOW_REQ_ERR_NOT_READY:
        return WORKFLOW_LOCAL_ACCESS_REJECT_NOT_READY;
    case WORKFLOW_REQ_ERR_BUSY:
    case WORKFLOW_REQ_ERR_SESSION_MISMATCH:
        return WORKFLOW_LOCAL_ACCESS_REJECT_BUSY;
    case WORKFLOW_REQ_ERR_STOP_GUARD:
        return WORKFLOW_LOCAL_ACCESS_REJECT_STOP_GUARD;
    default:
        return WORKFLOW_LOCAL_ACCESS_REJECT_INVALID;
    }
}

static void workflow_local_access_update_state(workflow_local_access_outcome_t outcome,
                                               uint32_t                       now_ms,
                                               const char                    *reason,
                                               const char                    *source_code,
                                               const char                    *session_ref,
                                               bool                           idempotent)
{
    s_state.last_outcome = outcome;
    s_state.last_decision_at_ms = now_ms;
    s_state.last_idempotent = idempotent;
    s_state.active_token_bound = workflow_local_access_has_active_token();

    workflow_local_access_copy_symbol(s_state.last_reason, sizeof(s_state.last_reason), reason);
    workflow_local_access_copy_symbol(s_state.last_source, sizeof(s_state.last_source), source_code);
    if (session_ref && session_ref[0] != '\0') {
        (void)strncpy(s_state.last_session_id, session_ref, sizeof(s_state.last_session_id) - 1U);
        s_state.last_session_id[sizeof(s_state.last_session_id) - 1U] = '\0';
    } else {
        s_state.last_session_id[0] = '\0';
    }
}

static void workflow_local_access_emit_event(const char *event_code,
                                             const char *source_code,
                                             const char *reason,
                                             const char *session_ref,
                                             bool        idempotent)
{
    char safe_source[WORKFLOW_LOCAL_ACCESS_SOURCE_LEN];
    char safe_reason[WORKFLOW_LOCAL_ACCESS_REASON_LEN];
    char payload[320];
    char event[640];
    int  built;

    workflow_local_access_copy_symbol(safe_source, sizeof(safe_source), source_code);
    workflow_local_access_copy_symbol(safe_reason, sizeof(safe_reason), reason);
    (void)snprintf(payload, sizeof(payload),
                   "\"access_source\":\"%s\","
                   "\"access_reason\":\"%s\","
                   "\"idempotent\":%s,"
                   "\"active_token_bound\":%s,"
                   "\"workflow_state\":\"%s\"",
                   safe_source[0] != '\0' ? safe_source : "local_token",
                   safe_reason,
                   idempotent ? "true" : "false",
                   workflow_local_access_has_active_token() ? "true" : "false",
                   workflow_control_state_label(workflow_engine_get_state()));

    built = proto_event_report_build(event, sizeof(event), session_ref, event_code, payload);
    if (built > 0) {
        (void)net_connectivity_send_json(event, (size_t)built);
    }
}

static void workflow_local_access_emit_voice_prompt(workflow_local_access_decision_t decision)
{
    static const char *not_ready_prompts[] = {"welcome", "starting_wait"};
    const char        *source = "local_access";

    switch (decision) {
    case WORKFLOW_LOCAL_ACCESS_ALLOW_START:
        workflow_voice_prompt_once("welcome", source, 800U);
        break;
    case WORKFLOW_LOCAL_ACCESS_REJECT_NOT_READY:
        workflow_voice_prompt_sequence(source, not_ready_prompts, 2U, 800U);
        break;
    case WORKFLOW_LOCAL_ACCESS_REJECT_BUSY:
        workflow_voice_prompt_once("port_busy", source, 1500U);
        break;
    case WORKFLOW_LOCAL_ACCESS_REJECT_STOP_GUARD:
        workflow_voice_prompt_once("port_busy", source, 1500U);
        break;
    case WORKFLOW_LOCAL_ACCESS_REJECT_INVALID:
        workflow_voice_prompt_once("invalid_card", source, 1500U);
        break;
    default:
        break;
    }
}

void workflow_local_access_init(void)
{
    memset(s_last_token, 0, sizeof(s_last_token));
    s_last_seen_at_ms = 0U;
    memset(s_active_token, 0, sizeof(s_active_token));
    memset(&s_state, 0, sizeof(s_state));
}

void workflow_local_access_get_policy(workflow_local_access_policy_t *out)
{
    if (!out) {
        return;
    }

    out->global_debounce_ms = WORKFLOW_LOCAL_ACCESS_GLOBAL_DEBOUNCE_MS;
    out->same_token_debounce_ms = WORKFLOW_LOCAL_ACCESS_SAME_TOKEN_DEBOUNCE_MS;
    out->post_stop_drop_window_ms = WORKFLOW_LOCAL_ACCESS_POST_STOP_DROP_MS;
}

void workflow_local_access_get_state(workflow_local_access_state_t *out)
{
    if (!out) {
        return;
    }

    s_state.active_token_bound = workflow_local_access_has_active_token();
    *out = s_state;
}

const char *workflow_local_access_outcome_label(workflow_local_access_outcome_t outcome)
{
    switch (outcome) {
    case WORKFLOW_LOCAL_ACCESS_OUTCOME_START_ACCEPTED:
        return "start_accepted";
    case WORKFLOW_LOCAL_ACCESS_OUTCOME_STOP_ACCEPTED:
        return "stop_accepted";
    case WORKFLOW_LOCAL_ACCESS_OUTCOME_REJECTED:
        return "rejected";
    default:
        return "none";
    }
}

bool workflow_local_access_has_active_token(void)
{
    return s_active_token[0] != '\0';
}

void workflow_local_access_clear_active_token(void)
{
    memset(s_active_token, 0, sizeof(s_active_token));
    s_state.active_token_bound = false;
}

workflow_local_access_decision_t workflow_local_access_evaluate(const char *token,
                                                                uint32_t    now_ms,
                                                                char       *reason,
                                                                size_t      reason_cap)
{
    workflow_state_t       state;
    device_runtime_t      *runtime;
    const common_status_t *status;

    if (!token || token[0] == '\0') {
        workflow_local_access_set_reason(reason, reason_cap, "missing_token");
        return WORKFLOW_LOCAL_ACCESS_REJECT_INVALID;
    }

    if (s_last_seen_at_ms != 0U) {
        uint32_t delta_ms = now_ms - s_last_seen_at_ms;
        if (delta_ms < WORKFLOW_LOCAL_ACCESS_GLOBAL_DEBOUNCE_MS) {
            workflow_local_access_set_reason(reason, reason_cap, "global_debounce");
            return WORKFLOW_LOCAL_ACCESS_REJECT_DEBOUNCE;
        }
        if (workflow_local_access_same_token(token, s_last_token) &&
            delta_ms < WORKFLOW_LOCAL_ACCESS_SAME_TOKEN_DEBOUNCE_MS) {
            workflow_local_access_set_reason(reason, reason_cap, "same_token_debounce");
            return WORKFLOW_LOCAL_ACCESS_REJECT_DEBOUNCE;
        }
    }

    workflow_local_access_note_seen(token, now_ms);

    state = workflow_engine_get_state();
    runtime = workflow_engine_runtime();
    status = common_status_get();

    if (workflow_engine_stop_guard_remaining_ms() > 0U ||
        (runtime && (runtime->recovery.recovery_pending || runtime->recovery.settlement_pending))) {
        workflow_local_access_set_reason(reason, reason_cap, "stop_guard_active");
        return WORKFLOW_LOCAL_ACCESS_REJECT_STOP_GUARD;
    }

    if (state == WF_RUNNING || state == WF_PAUSED) {
        if (workflow_local_access_same_token(token, s_active_token)) {
            workflow_local_access_set_reason(reason, reason_cap, "stop_session");
            return WORKFLOW_LOCAL_ACCESS_ALLOW_STOP;
        }

        workflow_local_access_set_reason(reason, reason_cap, "busy_with_other_session");
        return WORKFLOW_LOCAL_ACCESS_REJECT_BUSY;
    }

    if (state == WF_STARTING || state == WF_PAUSING || state == WF_RESUMING || state == WF_STOPPING) {
        workflow_local_access_set_reason(reason, reason_cap, "workflow_transition_busy");
        return WORKFLOW_LOCAL_ACCESS_REJECT_BUSY;
    }

    if (!status || !status->ready || state != WF_READY_IDLE) {
        workflow_local_access_set_reason(reason, reason_cap, "controller_not_ready");
        return WORKFLOW_LOCAL_ACCESS_REJECT_NOT_READY;
    }

    workflow_local_access_set_reason(reason, reason_cap, "start_session");
    return WORKFLOW_LOCAL_ACCESS_ALLOW_START;
}

workflow_local_access_decision_t workflow_local_access_submit_token(const char *token,
                                                                    const char *source_code,
                                                                    char       *reason,
                                                                    size_t      reason_cap,
                                                                    char       *session_ref,
                                                                    size_t      session_ref_cap,
                                                                    bool       *idempotent)
{
    workflow_local_access_decision_t decision;
    uint32_t                         now_ms = app_context()->monotonic_ms;
    const char                      *source = (source_code && source_code[0] != '\0') ? source_code : "local_token";
    char                             local_reason[WORKFLOW_LOCAL_ACCESS_REASON_LEN];
    char                             local_session_ref[MODEL_SESSION_ID_LEN];
    bool                             local_idempotent = false;

    local_reason[0] = '\0';
    local_session_ref[0] = '\0';
    if (idempotent) {
        *idempotent = false;
    }
    if (session_ref && session_ref_cap > 0U) {
        session_ref[0] = '\0';
    }

    decision = workflow_local_access_evaluate(token, now_ms, local_reason, sizeof(local_reason));
    if (decision == WORKFLOW_LOCAL_ACCESS_ALLOW_START) {
        int start_result =
            workflow_control_start_session(NULL, local_session_ref, sizeof(local_session_ref), &local_idempotent);
        if (start_result < 0) {
            workflow_local_access_set_reason(local_reason, sizeof(local_reason),
                                             workflow_control_error_message(start_result));
            decision = workflow_local_access_map_workflow_error(start_result);
            workflow_local_access_update_state(WORKFLOW_LOCAL_ACCESS_OUTCOME_REJECTED, now_ms, local_reason, source,
                                               local_session_ref, false);
            workflow_local_access_emit_event("local_access_rejected", source, local_reason, local_session_ref, false);
            workflow_local_access_emit_voice_prompt(decision);
        } else {
            workflow_local_access_note_start_accepted(token, now_ms);
            workflow_local_access_update_state(WORKFLOW_LOCAL_ACCESS_OUTCOME_START_ACCEPTED, now_ms,
                                               local_idempotent ? "idempotent_start" : "start_session", source,
                                               local_session_ref, local_idempotent);
            workflow_local_access_emit_event("local_access_start_accepted", source,
                                             local_idempotent ? "idempotent_start" : "start_session",
                                             local_session_ref, local_idempotent);
            workflow_local_access_set_reason(local_reason, sizeof(local_reason),
                                             local_idempotent ? "idempotent_start" : "start_session");
            workflow_local_access_emit_voice_prompt(decision);
        }
    } else if (decision == WORKFLOW_LOCAL_ACCESS_ALLOW_STOP) {
        device_runtime_t *runtime = workflow_engine_runtime();
        const char       *active_session_ref = (runtime && runtime->active_session.session_id[0] != '\0')
                                                 ? runtime->active_session.session_id
                                                 : NULL;
        int stop_result = workflow_control_stop_session(active_session_ref, &local_idempotent);

        if (active_session_ref) {
            (void)strncpy(local_session_ref, active_session_ref, sizeof(local_session_ref) - 1U);
            local_session_ref[sizeof(local_session_ref) - 1U] = '\0';
        }

        if (stop_result < 0) {
            workflow_local_access_set_reason(local_reason, sizeof(local_reason),
                                             workflow_control_error_message(stop_result));
            decision = workflow_local_access_map_workflow_error(stop_result);
            workflow_local_access_update_state(WORKFLOW_LOCAL_ACCESS_OUTCOME_REJECTED, now_ms, local_reason, source,
                                               local_session_ref, false);
            workflow_local_access_emit_event("local_access_rejected", source, local_reason, local_session_ref, false);
            workflow_local_access_emit_voice_prompt(decision);
        } else {
            workflow_local_access_note_stop_accepted(now_ms);
            workflow_local_access_update_state(WORKFLOW_LOCAL_ACCESS_OUTCOME_STOP_ACCEPTED, now_ms,
                                               local_idempotent ? "idempotent_stop" : "stop_session", source,
                                               local_session_ref, local_idempotent);
            workflow_local_access_emit_event("local_access_stop_accepted", source,
                                             local_idempotent ? "idempotent_stop" : "stop_session",
                                             local_session_ref, local_idempotent);
            workflow_local_access_set_reason(local_reason, sizeof(local_reason),
                                             local_idempotent ? "idempotent_stop" : "stop_session");
        }
    } else {
        workflow_local_access_update_state(WORKFLOW_LOCAL_ACCESS_OUTCOME_REJECTED, now_ms, local_reason, source, NULL,
                                           false);
        workflow_local_access_emit_event("local_access_rejected", source, local_reason, NULL, false);
        workflow_local_access_emit_voice_prompt(decision);
    }

    if (reason && reason_cap > 0U) {
        workflow_local_access_set_reason(reason, reason_cap, local_reason);
    }
    if (session_ref && session_ref_cap > 0U && local_session_ref[0] != '\0') {
        (void)strncpy(session_ref, local_session_ref, session_ref_cap - 1U);
        session_ref[session_ref_cap - 1U] = '\0';
    }
    if (idempotent) {
        *idempotent = local_idempotent;
    }
    return decision;
}

void workflow_local_access_note_start_accepted(const char *token, uint32_t now_ms)
{
    workflow_local_access_note_seen(token, now_ms);
    if (!token || token[0] == '\0') {
        workflow_local_access_clear_active_token();
        return;
    }

    (void)strncpy(s_active_token, token, sizeof(s_active_token) - 1U);
    s_active_token[sizeof(s_active_token) - 1U] = '\0';
    s_state.active_token_bound = true;
    s_state.last_seen_at_ms = now_ms;
}

void workflow_local_access_note_stop_accepted(uint32_t now_ms)
{
    s_last_seen_at_ms = now_ms;
    workflow_local_access_clear_active_token();
    s_state.last_seen_at_ms = now_ms;
}
