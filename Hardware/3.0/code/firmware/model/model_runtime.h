/**
 * Volatile / persisted runtime — sessions, recovery, counters — Firmware Dev Spec v1 §9, §11.
 */
#ifndef MODEL_RUNTIME_H
#define MODEL_RUNTIME_H

#include "model_types.h"
#include <stdint.h>
#include <stdbool.h>

#define MODEL_SESSION_ID_LEN 64U
#define MODEL_RECOVERY_HINT_LEN 64U

typedef struct {
    char     session_id[MODEL_SESSION_ID_LEN];
    uint32_t started_at_utc;
    uint8_t  reserved[8];
} workflow_session_context_t;

typedef struct {
    bool     recovery_pending;
    bool     settlement_pending;
    uint32_t last_stop_reason_code;
    uint32_t last_stop_at_utc;
    uint32_t last_session_started_at_utc;
    char     last_session_id[MODEL_SESSION_ID_LEN];
    char     last_recovery_hint[MODEL_RECOVERY_HINT_LEN];
} workflow_recovery_context_t;

typedef struct {
    workflow_state_t           workflow_state;
    workflow_session_context_t active_session;
    workflow_recovery_context_t recovery;
    uint64_t                   flow_pulse_total;
    double                     energy_kwh_total;
} device_runtime_t;

#endif /* MODEL_RUNTIME_H */
