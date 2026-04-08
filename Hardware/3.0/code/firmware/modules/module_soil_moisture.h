#ifndef MODULE_SOIL_MOISTURE_H
#define MODULE_SOIL_MOISTURE_H

#include "model_types.h"
#include <stdint.h>

typedef struct {
    uint8_t probe_id;
} module_soil_moisture_config_t;

typedef struct {
    float   soil_moisture_vwc;
    uint8_t quality;
} module_soil_moisture_values_t;

void module_soil_moisture_init(void);
void module_soil_moisture_tick_100ms(void);
void module_soil_moisture_tick_1s(void);
uint8_t module_soil_moisture_apply_config(const module_soil_moisture_config_t *cfg);
uint8_t module_soil_moisture_query_state(void *out);
uint8_t module_soil_moisture_query_values(void *out);
uint8_t module_soil_moisture_execute_action(const char *action_code, const char *target_ref, const void *payload);

const module_ops_t *module_soil_moisture_ops(void);

#endif /* MODULE_SOIL_MOISTURE_H */
