#ifndef MODULE_METER_H
#define MODULE_METER_H

#include "model_types.h"
#include <stdint.h>

#define MODULE_METER_PROTOCOL_UNKNOWN      0U
#define MODULE_METER_PROTOCOL_DLT645_2007  1U

typedef struct {
    uint8_t  slave_addr;
    uint8_t  addr_bcd[6];
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
uint8_t module_meter_force_refresh(void);
uint8_t module_meter_execute_action(const char *action_code, const char *target_ref, const void *payload);
const char *module_meter_source_name(void);
uint8_t module_meter_get_identity(uint8_t *protocol_variant, uint8_t addr_bcd[6], uint8_t *addr_valid);

const module_ops_t *module_meter_ops(void);

#endif /* MODULE_METER_H */
