/**
 * Workflow state machine.
 */
#ifndef WORKFLOW_ENGINE_H
#define WORKFLOW_ENGINE_H

#include "model_runtime.h"
#include <stdbool.h>
#include <stdint.h>

#define WORKFLOW_REQ_OK                    0
#define WORKFLOW_REQ_IDEMPOTENT            1
#define WORKFLOW_REQ_ERR_INVALID_ARG      -1
#define WORKFLOW_REQ_ERR_NOT_READY        -2
#define WORKFLOW_REQ_ERR_BUSY             -3
#define WORKFLOW_REQ_ERR_DEBOUNCE         -4
#define WORKFLOW_REQ_ERR_STOP_GUARD       -5
#define WORKFLOW_REQ_ERR_SESSION_MISMATCH -6

void workflow_engine_init(void);

workflow_state_t workflow_engine_get_state(void);
void             workflow_engine_set_state(workflow_state_t st);

device_runtime_t *workflow_engine_runtime(void);

void workflow_engine_poll_ready(const runtime_rules_t *rules, bool config_loaded, bool key_modules_ok);

int workflow_engine_request_start_session(const workflow_session_context_t *session);
int workflow_engine_request_stop_session(const char *session_ref);
int workflow_engine_request_pause_session(void);
int workflow_engine_request_resume_session(void);

void workflow_engine_tick_ms(uint32_t monotonic_ms);

bool workflow_engine_recovery_pending(void);
void workflow_engine_set_recovery_pending(bool v);
void workflow_engine_mark_recovery_reported(void);
uint32_t workflow_engine_stop_guard_remaining_ms(void);

#endif /* WORKFLOW_ENGINE_H */
