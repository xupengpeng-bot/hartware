#include "workflow_upgrade_guard.h"

static bool s_workflow_running;
static bool s_settlement_pending;

void workflow_upgrade_guard_init(void)
{
    s_workflow_running = false;
    s_settlement_pending = false;
}

void workflow_upgrade_guard_set_running(bool running)
{
    s_workflow_running = running;
}

void workflow_upgrade_guard_set_settlement_pending(bool pending)
{
    s_settlement_pending = pending;
}

uint32_t workflow_upgrade_guard_workflow_state_for_event(void)
{
    if (s_workflow_running) {
        return 1U;
    }
    return 0U;
}

bool workflow_upgrade_guard_allow_upgrade(bool allow_running_override,
                                          workflow_upgrade_deny_reason_t *out_reason)
{
    if (s_settlement_pending) {
        if (out_reason) {
            *out_reason = WORKFLOW_UPGRADE_DENY_SETTLEMENT_PENDING;
        }
        return false;
    }
    if (s_workflow_running && !allow_running_override) {
        if (out_reason) {
            *out_reason = WORKFLOW_UPGRADE_DENY_RUNNING;
        }
        return false;
    }
    if (out_reason) {
        *out_reason = WORKFLOW_UPGRADE_OK;
    }
    return true;
}
