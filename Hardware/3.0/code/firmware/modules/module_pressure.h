#ifndef MODULE_PRESSURE_H
#define MODULE_PRESSURE_H

#include "model_types.h"
#include <stdint.h>

typedef struct {
    uint8_t  input_mode;
    float    zero_offset;
    float    full_scale;
    uint16_t sample_interval_ms;
} module_pressure_config_t;

typedef struct {
    float    pressure_mpa;
    uint8_t  quality;
    uint32_t fault_codes[4];
    uint8_t  fault_count;
} module_pressure_values_t;

void module_pressure_init(void);
void module_pressure_tick_100ms(void);
void module_pressure_tick_1s(void);
uint8_t module_pressure_apply_config(const module_pressure_config_t *cfg);
uint8_t module_pressure_query_state(void *out);
uint8_t module_pressure_query_values(void *out);
uint8_t module_pressure_execute_action(const char *action_code, const char *target_ref, const void *payload);

const module_ops_t *module_pressure_ops(void);
int                 module_pressure_get_mpa(float *out);

#endif /* MODULE_PRESSURE_H */
