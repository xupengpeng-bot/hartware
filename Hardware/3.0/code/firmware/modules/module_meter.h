#ifndef MODULE_METER_H
#define MODULE_METER_H

#include "model_types.h"
#include <stdint.h>

typedef struct {
    uint8_t  slave_addr;
    uint32_t baudrate;
    uint8_t  protocol_variant;
} module_meter_config_t;

typedef struct {
    double energy_kwh;
    double power_kw;
    double voltage_v;
    double current_a;
    uint8_t quality;
} module_meter_values_t;

void module_meter_init(void);
void module_meter_tick_100ms(void);
void module_meter_tick_1s(void);
uint8_t module_meter_apply_config(const module_meter_config_t *cfg);
uint8_t module_meter_query_state(void *out);
uint8_t module_meter_query_values(void *out);
uint8_t module_meter_execute_action(const char *action_code, const char *target_ref, const void *payload);

const module_ops_t *module_meter_ops(void);

#endif /* MODULE_METER_H */
