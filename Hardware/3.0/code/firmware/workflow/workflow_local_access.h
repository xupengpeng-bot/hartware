#ifndef WORKFLOW_LOCAL_ACCESS_H
#define WORKFLOW_LOCAL_ACCESS_H

#include "model_runtime.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WORKFLOW_LOCAL_ACCESS_TOKEN_LEN 48U
#define WORKFLOW_LOCAL_ACCESS_REASON_LEN 48U
#define WORKFLOW_LOCAL_ACCESS_SOURCE_LEN 24U

typedef enum {
    WORKFLOW_LOCAL_ACCESS_ALLOW_START = 1,
    WORKFLOW_LOCAL_ACCESS_ALLOW_STOP = 2,
    WORKFLOW_LOCAL_ACCESS_REJECT_INVALID = -1,
    WORKFLOW_LOCAL_ACCESS_REJECT_DEBOUNCE = -2,
    WORKFLOW_LOCAL_ACCESS_REJECT_NOT_READY = -3,
    WORKFLOW_LOCAL_ACCESS_REJECT_BUSY = -4,
    WORKFLOW_LOCAL_ACCESS_REJECT_STOP_GUARD = -5
} workflow_local_access_decision_t;

typedef enum {
    WORKFLOW_LOCAL_ACCESS_OUTCOME_NONE = 0,
    WORKFLOW_LOCAL_ACCESS_OUTCOME_START_ACCEPTED = 1,
    WORKFLOW_LOCAL_ACCESS_OUTCOME_STOP_ACCEPTED = 2,
    WORKFLOW_LOCAL_ACCESS_OUTCOME_REJECTED = 3
} workflow_local_access_outcome_t;

typedef struct {
    uint32_t global_debounce_ms;
    uint32_t same_token_debounce_ms;
    uint32_t post_stop_drop_window_ms;
} workflow_local_access_policy_t;

typedef struct {
    workflow_local_access_outcome_t last_outcome;
    uint32_t                        last_seen_at_ms;
    uint32_t                        last_decision_at_ms;
    bool                            last_idempotent;
    bool                            active_token_bound;
    char                            last_reason[WORKFLOW_LOCAL_ACCESS_REASON_LEN];
    char                            last_source[WORKFLOW_LOCAL_ACCESS_SOURCE_LEN];
    char                            last_session_id[MODEL_SESSION_ID_LEN];
} workflow_local_access_state_t;

void workflow_local_access_init(void);
void workflow_local_access_get_policy(workflow_local_access_policy_t *out);
void workflow_local_access_get_state(workflow_local_access_state_t *out);
const char *workflow_local_access_outcome_label(workflow_local_access_outcome_t outcome);
bool workflow_local_access_has_active_token(void);
void workflow_local_access_clear_active_token(void);
workflow_local_access_decision_t workflow_local_access_evaluate(const char *token,
                                                                uint32_t    now_ms,
                                                                char       *reason,
                                                                size_t      reason_cap);
workflow_local_access_decision_t workflow_local_access_submit_token(const char *token,
                                                                    const char *source_code,
                                                                    char       *reason,
                                                                    size_t      reason_cap,
                                                                    char       *session_ref,
                                                                    size_t      session_ref_cap,
                                                                    bool       *idempotent);
void workflow_local_access_note_start_accepted(const char *token, uint32_t now_ms);
void workflow_local_access_note_stop_accepted(uint32_t now_ms);

#endif /* WORKFLOW_LOCAL_ACCESS_H */
