#ifndef SAFETY_FLOW_H
#define SAFETY_FLOW_H

#include "model_types.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *resource_lock;
    uint32_t timeout_ms;
    const char *conflicts_with;
    uint8_t interruptible;
} safety_action_meta_t;

void safety_flow_init(void);
void safety_flow_tick(uint32_t monotonic_ms);
void safety_flow_poll_ready(void);
void safety_flow_poll_protection(uint32_t monotonic_ms);
void safety_flow_on_query_result(const char *json);
int  safety_flow_on_card_read(const char *card_no);
int  safety_flow_execute_action(const char *action_code, const char *json,
                                char *detail_json, size_t detail_cap);
const char *safety_flow_error_message(int code);

int safety_flow_build_blocked_json(char *buf, size_t cap,
                                   const char *blocking_resource,
                                   const char *blocking_action,
                                   uint32_t retry_after_ms);

const char *safety_flow_state_name(void);

#endif /* SAFETY_FLOW_H */
