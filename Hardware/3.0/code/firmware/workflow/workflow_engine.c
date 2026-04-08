#include "workflow_engine.h"
#include "workflow_local_access.h"
#include "../common/common_status.h"
#include "../storage/storage_runtime.h"
#include "../../workflow/workflow_upgrade_guard.h"

#include <string.h>

static device_runtime_t s_rt;
static uint32_t         s_now_ms;
static uint32_t         s_last_action_ms;
static uint32_t         s_stop_guard_until_ms;

#define WORKFLOW_ACTION_DEBOUNCE_MS 2000U
#define WORKFLOW_STOP_GUARD_MS      5000U
#define WORKFLOW_STOP_REASON_MANUAL     1U
#define WORKFLOW_STOP_REASON_POWER_LOSS 1001U

static int workflow_session_matches(const char *left, const char *right)
{
    if (!left || !right || left[0] == '\0' || right[0] == '\0') {
        return 0;
    }
    return strcmp(left, right) == 0;
}

static bool workflow_deadline_active(uint32_t deadline_ms)
{
    return deadline_ms != 0U && (int32_t)(deadline_ms - s_now_ms) > 0;
}

static void workflow_persist(void)
{
    (void)storage_runtime_save(&s_rt);
}

static void workflow_capture_session_for_recovery(const workflow_session_context_t *session,
                                                  uint32_t                          stop_reason_code,
                                                  const char                       *hint)
{
    if (session && session->session_id[0] != '\0') {
        (void)strncpy(s_rt.recovery.last_session_id, session->session_id,
                      sizeof(s_rt.recovery.last_session_id) - 1U);
        s_rt.recovery.last_session_id[sizeof(s_rt.recovery.last_session_id) - 1U] = '\0';
        s_rt.recovery.last_session_started_at_utc = session->started_at_utc;
    }

    s_rt.recovery.last_stop_reason_code = stop_reason_code;
    s_rt.recovery.last_stop_at_utc = s_now_ms / 1000U;
    s_rt.recovery.settlement_pending = true;
    workflow_upgrade_guard_set_settlement_pending(true);
    s_stop_guard_until_ms = s_now_ms + WORKFLOW_STOP_GUARD_MS;

    if (hint && hint[0] != '\0') {
        (void)strncpy(s_rt.recovery.last_recovery_hint, hint, sizeof(s_rt.recovery.last_recovery_hint) - 1U);
        s_rt.recovery.last_recovery_hint[sizeof(s_rt.recovery.last_recovery_hint) - 1U] = '\0';
    } else {
        s_rt.recovery.last_recovery_hint[0] = '\0';
    }
}

static void workflow_finish_settlement_if_expired(void)
{
    if (!s_rt.recovery.settlement_pending) {
        return;
    }
    if (workflow_deadline_active(s_stop_guard_until_ms)) {
        return;
    }

    s_rt.recovery.settlement_pending = false;
    workflow_upgrade_guard_set_settlement_pending(false);
    s_stop_guard_until_ms = 0U;
    workflow_persist();
}

static void workflow_promote_dirty_session_to_recovery(void)
{
    if (s_rt.active_session.session_id[0] == '\0') {
        return;
    }

    workflow_session_context_t dirty_session = s_rt.active_session;
    workflow_capture_session_for_recovery(&dirty_session, WORKFLOW_STOP_REASON_POWER_LOSS, "power_loss_stop");
    s_rt.recovery.recovery_pending = true;
    workflow_local_access_clear_active_token();
    memset(&s_rt.active_session, 0, sizeof(s_rt.active_session));
    s_rt.workflow_state = WF_STOPPED;
}

static void workflow_restore_runtime(void)
{
    device_runtime_t persisted;

    memset(&s_rt, 0, sizeof(s_rt));
    s_stop_guard_until_ms = 0U;

    if (storage_runtime_load(&persisted) != 0) {
        s_rt.workflow_state = WF_BOOTING;
        workflow_upgrade_guard_set_settlement_pending(false);
        return;
    }

    s_rt = persisted;
    if (s_rt.workflow_state > WF_ERROR_STOP) {
        s_rt.workflow_state = WF_BOOTING;
    }

    if (s_rt.recovery.settlement_pending) {
        s_stop_guard_until_ms = WORKFLOW_STOP_GUARD_MS;
    }

    workflow_promote_dirty_session_to_recovery();
    workflow_upgrade_guard_set_settlement_pending(s_rt.recovery.settlement_pending);
    workflow_engine_set_state(s_rt.workflow_state);
    workflow_persist();
}

void workflow_engine_tick_ms(uint32_t monotonic_ms)
{
    s_now_ms = monotonic_ms;
    workflow_finish_settlement_if_expired();
}

void workflow_engine_init(void)
{
    workflow_upgrade_guard_init();
    workflow_restore_runtime();
}

workflow_state_t workflow_engine_get_state(void)
{
    return s_rt.workflow_state;
}

void workflow_engine_set_state(workflow_state_t st)
{
    s_rt.workflow_state = st;
    {
        bool session_active =
            (st == WF_STARTING || st == WF_RUNNING || st == WF_PAUSING || st == WF_PAUSED || st == WF_RESUMING ||
             st == WF_STOPPING);
        workflow_upgrade_guard_set_running(session_active);
    }
}

device_runtime_t *workflow_engine_runtime(void)
{
    return &s_rt;
}

void workflow_engine_poll_ready(const runtime_rules_t *rules, bool config_loaded, bool key_modules_ok)
{
    (void)rules;
    workflow_finish_settlement_if_expired();

    {
        const common_status_t *cs = common_status_get();
        bool net_ok = cs->online && cs->tcp_connected && cs->registered_once;
        bool ready = net_ok && config_loaded && key_modules_ok && !s_rt.recovery.recovery_pending &&
                     !s_rt.recovery.settlement_pending;

        common_status_set_ready(ready);

        if (ready) {
            if (s_rt.workflow_state == WF_BOOTING || s_rt.workflow_state == WF_ONLINE_NOT_READY ||
                s_rt.workflow_state == WF_STOPPED) {
                workflow_engine_set_state(WF_READY_IDLE);
            }
        } else if (net_ok) {
            if (s_rt.workflow_state == WF_BOOTING || s_rt.workflow_state == WF_ONLINE_NOT_READY) {
                workflow_engine_set_state(WF_ONLINE_NOT_READY);
            }
        }
    }
}

int workflow_engine_request_start_session(const workflow_session_context_t *session)
{
    const common_status_t *cs;

    if (!session) {
        return WORKFLOW_REQ_ERR_INVALID_ARG;
    }

    workflow_finish_settlement_if_expired();

    if (workflow_session_matches(session->session_id, s_rt.active_session.session_id) &&
        (s_rt.workflow_state == WF_STARTING || s_rt.workflow_state == WF_RUNNING || s_rt.workflow_state == WF_PAUSING ||
         s_rt.workflow_state == WF_PAUSED || s_rt.workflow_state == WF_RESUMING)) {
        return WORKFLOW_REQ_IDEMPOTENT;
    }

    if (s_last_action_ms != 0U) {
        uint32_t delta_ms = s_now_ms - s_last_action_ms;
        if (delta_ms < WORKFLOW_ACTION_DEBOUNCE_MS) {
            return WORKFLOW_REQ_ERR_DEBOUNCE;
        }
    }

    if (workflow_deadline_active(s_stop_guard_until_ms) || s_rt.recovery.recovery_pending ||
        s_rt.recovery.settlement_pending) {
        return WORKFLOW_REQ_ERR_STOP_GUARD;
    }

    cs = common_status_get();
    if (s_rt.workflow_state == WF_STOPPED && cs->ready) {
        workflow_engine_set_state(WF_READY_IDLE);
    }

    if (!cs->ready || s_rt.workflow_state != WF_READY_IDLE) {
        return WORKFLOW_REQ_ERR_NOT_READY;
    }

    if (s_rt.active_session.session_id[0] != '\0') {
        return WORKFLOW_REQ_ERR_BUSY;
    }

    s_rt.active_session = *session;
    if (s_rt.active_session.started_at_utc == 0U) {
        s_rt.active_session.started_at_utc = s_now_ms / 1000U;
    }

    workflow_engine_set_state(WF_STARTING);
    workflow_engine_set_state(WF_RUNNING);
    s_last_action_ms = s_now_ms;
    workflow_persist();
    return WORKFLOW_REQ_OK;
}

int workflow_engine_request_stop_session(const char *session_ref)
{
    workflow_finish_settlement_if_expired();

    if ((s_rt.workflow_state == WF_STOPPING || s_rt.workflow_state == WF_STOPPED) &&
        ((session_ref == NULL || session_ref[0] == '\0') ||
         workflow_session_matches(session_ref, s_rt.recovery.last_session_id) ||
         workflow_session_matches(session_ref, s_rt.active_session.session_id))) {
        return WORKFLOW_REQ_IDEMPOTENT;
    }

    if (s_last_action_ms != 0U) {
        uint32_t delta_ms = s_now_ms - s_last_action_ms;
        if (delta_ms < WORKFLOW_ACTION_DEBOUNCE_MS) {
            return WORKFLOW_REQ_ERR_DEBOUNCE;
        }
    }

    if (s_rt.workflow_state != WF_RUNNING && s_rt.workflow_state != WF_PAUSED) {
        return WORKFLOW_REQ_ERR_INVALID_ARG;
    }

    if (session_ref && session_ref[0] != '\0' &&
        !workflow_session_matches(session_ref, s_rt.active_session.session_id)) {
        return WORKFLOW_REQ_ERR_SESSION_MISMATCH;
    }

    {
        workflow_session_context_t stopping_session = s_rt.active_session;
        workflow_engine_set_state(WF_STOPPING);
        workflow_capture_session_for_recovery(&stopping_session, WORKFLOW_STOP_REASON_MANUAL, "manual_stop");
        workflow_local_access_clear_active_token();
        memset(&s_rt.active_session, 0, sizeof(s_rt.active_session));
    }

    workflow_engine_set_state(WF_STOPPED);
    s_last_action_ms = s_now_ms;
    workflow_persist();
    return WORKFLOW_REQ_OK;
}

int workflow_engine_request_pause_session(void)
{
    if (s_last_action_ms != 0U) {
        uint32_t delta_ms = s_now_ms - s_last_action_ms;
        if (delta_ms < WORKFLOW_ACTION_DEBOUNCE_MS) {
            return WORKFLOW_REQ_ERR_DEBOUNCE;
        }
    }

    if (s_rt.workflow_state != WF_RUNNING) {
        return WORKFLOW_REQ_ERR_INVALID_ARG;
    }

    workflow_engine_set_state(WF_PAUSING);
    workflow_engine_set_state(WF_PAUSED);
    s_last_action_ms = s_now_ms;
    workflow_persist();
    return WORKFLOW_REQ_OK;
}

int workflow_engine_request_resume_session(void)
{
    if (s_last_action_ms != 0U) {
        uint32_t delta_ms = s_now_ms - s_last_action_ms;
        if (delta_ms < WORKFLOW_ACTION_DEBOUNCE_MS) {
            return WORKFLOW_REQ_ERR_DEBOUNCE;
        }
    }

    if (s_rt.workflow_state != WF_PAUSED) {
        return WORKFLOW_REQ_ERR_INVALID_ARG;
    }

    workflow_engine_set_state(WF_RESUMING);
    workflow_engine_set_state(WF_RUNNING);
    s_last_action_ms = s_now_ms;
    workflow_persist();
    return WORKFLOW_REQ_OK;
}

bool workflow_engine_recovery_pending(void)
{
    return s_rt.recovery.recovery_pending;
}

void workflow_engine_set_recovery_pending(bool v)
{
    s_rt.recovery.recovery_pending = v;
    workflow_persist();
}

void workflow_engine_mark_recovery_reported(void)
{
    s_rt.recovery.recovery_pending = false;
    (void)strncpy(s_rt.recovery.last_recovery_hint, "reported", sizeof(s_rt.recovery.last_recovery_hint) - 1U);
    s_rt.recovery.last_recovery_hint[sizeof(s_rt.recovery.last_recovery_hint) - 1U] = '\0';
    workflow_persist();
}

uint32_t workflow_engine_stop_guard_remaining_ms(void)
{
    if (!workflow_deadline_active(s_stop_guard_until_ms)) {
        return 0U;
    }
    return s_stop_guard_until_ms - s_now_ms;
}
