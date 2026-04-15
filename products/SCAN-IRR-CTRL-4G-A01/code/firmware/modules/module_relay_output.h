#ifndef MODULE_RELAY_OUTPUT_H
#define MODULE_RELAY_OUTPUT_H

#include "model_types.h"
#include <stdint.h>

typedef enum {
    RELAY_OUTPUT_OFF = 0,
    RELAY_OUTPUT_ON
} module_relay_output_state_t;

void module_relay_output_init(void);
void module_relay_output_tick_100ms(void);
void module_relay_output_tick_1s(void);
uint8_t module_relay_output_apply_config(const void *cfg);
uint8_t module_relay_output_query_state(void *out);
uint8_t module_relay_output_query_values(void *out);
uint8_t module_relay_output_execute_action(const char *action_code, const char *target_ref, const void *payload);
int     module_relay_output_query_state_by_target(const char *target_ref, module_relay_output_state_t *out_state);

const module_ops_t *module_relay_output_ops(void);

#endif /* MODULE_RELAY_OUTPUT_H */
