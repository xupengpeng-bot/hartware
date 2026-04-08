#ifndef MODULE_FLOW_H
#define MODULE_FLOW_H

#include "model_types.h"
#include <stdint.h>

typedef struct {
    uint8_t  flow_input_type;
    float    k_factor;
    uint16_t sample_interval_ms;
} module_flow_config_t;

typedef struct {
    float    instant_m3h;
    double   total_m3;
    uint8_t  quality;
} module_flow_values_t;

void module_flow_init(void);
void module_flow_tick_100ms(void);
void module_flow_tick_1s(void);
uint8_t module_flow_apply_config(const module_flow_config_t *cfg);
uint8_t module_flow_query_state(void *out);
uint8_t module_flow_query_values(void *out);
uint8_t module_flow_execute_action(const char *action_code, const char *target_ref, const void *payload);

const module_ops_t *module_flow_ops(void);
int                 module_flow_get_instant(float *out_m3h);

#endif /* MODULE_FLOW_H */
