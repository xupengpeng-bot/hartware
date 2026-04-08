#include "workflow_recovery.h"
#include "workflow_engine.h"
#include "proto_event_report.h"
#include "net_connectivity.h"
#include "workflow_voice.h"

#include <string.h>
#include <stdio.h>

void workflow_recovery_boot_check(void)
{
    device_runtime_t *runtime = workflow_engine_runtime();
    const char       *session_ref;
    uint32_t          started_at_utc;
    char              payload[448];
    char              buf[768];
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

    (void)memset(payload, 0, sizeof(payload));
    (void)memset(buf, 0, sizeof(buf));
    (void)snprintf(payload, sizeof(payload),
                   "\"abnormal_stop\":true,"
                   "\"stop_reason_code\":\"power_loss_stop\","
                   "\"dirty_session\":true,"
                   "\"started_at_utc\":%lu,"
                   "\"last_stop_at_utc\":%lu,"
                   "\"last_stop_reason_code\":%lu,"
                   "\"last_recovery_hint\":\"%s\","
                   "\"recovery_pending\":true,"
                   "\"settlement_pending\":%s",
                   (unsigned long)started_at_utc,
                   (unsigned long)runtime->recovery.last_stop_at_utc,
                   (unsigned long)runtime->recovery.last_stop_reason_code,
                   runtime->recovery.last_recovery_hint[0] ? runtime->recovery.last_recovery_hint : "",
                   runtime->recovery.settlement_pending ? "true" : "false");

    built = proto_event_report_build(buf, sizeof(buf), session_ref, "power_loss_stop", payload);
    if (built <= 0) {
        return;
    }
    if (net_connectivity_send_json(buf, (size_t)built) < 0) {
        return;
    }

    workflow_voice_prompt_once("device_fault", "recovery", 1000U);
    workflow_voice_prompt_once("irrigation_finished", "recovery", 1000U);
    workflow_engine_mark_recovery_reported();
}
