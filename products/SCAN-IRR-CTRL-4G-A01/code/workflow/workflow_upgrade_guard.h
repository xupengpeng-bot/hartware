/**
 * workflow_upgrade_guard — whether business workflow allows OTA (Spec §8–9).
 */
#ifndef WORKFLOW_UPGRADE_GUARD_H
#define WORKFLOW_UPGRADE_GUARD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WORKFLOW_UPGRADE_OK = 0,
    WORKFLOW_UPGRADE_DENY_RUNNING,
    WORKFLOW_UPGRADE_DENY_SETTLEMENT_PENDING,
    WORKFLOW_UPGRADE_DENY_UNKNOWN,
} workflow_upgrade_deny_reason_t;

void workflow_upgrade_guard_init(void);

/** Platform hooks: set from irrigation/workflow engine. */
void workflow_upgrade_guard_set_running(bool running);
void workflow_upgrade_guard_set_settlement_pending(bool pending);

uint32_t workflow_upgrade_guard_workflow_state_for_event(void);

bool workflow_upgrade_guard_allow_upgrade(bool allow_running_override,
                                          workflow_upgrade_deny_reason_t *out_reason);

#endif /* WORKFLOW_UPGRADE_GUARD_H */
