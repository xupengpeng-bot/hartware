#ifndef WORKFLOW_CONTROL_H
#define WORKFLOW_CONTROL_H

#include "workflow_engine.h"
#include <stdbool.h>
#include <stddef.h>

const char *workflow_control_error_message(int code);
const char *workflow_control_state_label(workflow_state_t state);

int workflow_control_start_session(const char *preferred_session_ref,
                                   char       *resolved_session_ref,
                                   size_t      resolved_session_ref_cap,
                                   bool       *idempotent);
int workflow_control_stop_session(const char *session_ref, bool *idempotent);

#endif /* WORKFLOW_CONTROL_H */
