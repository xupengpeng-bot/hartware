#ifndef MODULE_PUMP_VFD_H
#define MODULE_PUMP_VFD_H

#include "model_types.h"
#include <stdint.h>

typedef enum {
    PUMP_VFD_IDLE = 0,
    PUMP_VFD_STARTING,
    PUMP_VFD_RUNNING,
    PUMP_VFD_STOPPING,
    PUMP_VFD_FAULT
} module_pump_vfd_state_t;

typedef struct {
    uint16_t start_timeout_ms;
    uint16_t stop_timeout_ms;
} module_pump_vfd_config_t;

void module_pump_vfd_init(void);
void module_pump_vfd_tick_100ms(void);
void module_pump_vfd_tick_1s(void);
uint8_t module_pump_vfd_apply_config(const module_pump_vfd_config_t *cfg);
uint8_t module_pump_vfd_query_state(void *out);
uint8_t module_pump_vfd_query_values(void *out);
uint8_t module_pump_vfd_execute_action(const char *action_code, const char *target_ref, const void *payload);

const module_ops_t *module_pump_vfd_ops(void);
int                 module_pump_vfd_query_state_u8(uint8_t *out_state);

#endif /* MODULE_PUMP_VFD_H */
