#include "workflow_recovery.h"
#include "workflow_engine.h"
#include "proto_event_report.h"
#include "workflow_voice.h"

#include <string.h>
#include <stdio.h>

void workflow_recovery_boot_check(void)
{
    device_runtime_t *runtime = workflow_engine_runtime();
    const char       *session_ref;
    uint32_t          started_at_utc;
    int               built;

    if (!runtime || !runtime->recovery.recovery_pending) {
        return;
    }

    session_ref = runtime->recovery.last_session_id[0] ? runtime->recovery.last_session_id : NULL;
    if (!session_ref) {
        workflow_engine_mark_recovery_reported();
        return;
    }

    started_at_utc = runtime->recovery.last_session_started_at_utc;
    (void)started_at_utc;

    built = proto_event_report_send_min(session_ref,
                                        "pls",
                                        "power_loss_stop",
                                        runtime->recovery.last_recovery_hint[0] ? runtime->recovery.last_recovery_hint : NULL,
                                        "pump_1");
    if (built < 0) {
        return;
    }

    workflow_voice_prompt_once("device_fault", "recovery", 1000U);
    workflow_voice_prompt_once("irrigation_finished", "recovery", 1000U);
    workflow_engine_mark_recovery_reported();
}
