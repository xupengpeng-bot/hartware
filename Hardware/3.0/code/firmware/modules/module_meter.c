#include "module_meter.h"
#include <string.h>

static module_meter_config_t s_cfg;
static module_meter_values_t s_val;

void module_meter_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_val, 0, sizeof(s_val));
    s_cfg.baudrate = 9600U;
}

void module_meter_tick_100ms(void)
{
}

void module_meter_tick_1s(void)
{
    /* Modbus read placeholder */
    s_val.quality = 1U;
}

uint8_t module_meter_apply_config(const module_meter_config_t *cfg)
{
    if (!cfg) {
        return 1U;
    }
    s_cfg = *cfg;
    return 0U;
}

uint8_t module_meter_query_state(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_meter_query_values(void *out)
{
    if (!out) {
        return 1U;
    }
    *(module_meter_values_t *)out = s_val;
    return 0U;
}

uint8_t module_meter_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    (void)action_code;
    (void)target_ref;
    (void)payload;
    return 1U;
}

static const module_ops_t s_ops = {
    .module_code    = "electric_meter_modbus",
    .init           = module_meter_init,
    .tick_100ms     = module_meter_tick_100ms,
    .tick_1s        = module_meter_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_meter_apply_config,
    .query_state    = module_meter_query_state,
    .query_values   = module_meter_query_values,
    .execute_action = module_meter_execute_action,
};

const module_ops_t *module_meter_ops(void)
{
    return &s_ops;
}
