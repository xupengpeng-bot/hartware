#ifndef MODULE_SINGLE_VALVE_H
#define MODULE_SINGLE_VALVE_H

#include "model_types.h"
#include <stdint.h>

typedef enum {
    VALVE_CLOSED = 0,
    VALVE_OPENING,
    VALVE_OPEN,
    VALVE_CLOSING,
    VALVE_ACTION_TIMEOUT
} module_valve_state_t;

typedef struct {
    uint16_t action_timeout_ms;
} module_single_valve_config_t;

void module_single_valve_init(void);
void module_single_valve_tick_100ms(void);
void module_single_valve_tick_1s(void);
uint8_t module_single_valve_apply_config(const module_single_valve_config_t *cfg);
uint8_t module_single_valve_query_state(void *out);
uint8_t module_single_valve_query_values(void *out);
uint8_t module_single_valve_execute_action(const char *action_code, const char *target_ref, const void *payload);

const module_ops_t *module_single_valve_ops(void);

#endif /* MODULE_SINGLE_VALVE_H */
