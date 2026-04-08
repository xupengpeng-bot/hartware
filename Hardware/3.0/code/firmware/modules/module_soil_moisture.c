#include "module_soil_moisture.h"
#include <string.h>

static module_soil_moisture_config_t  s_cfg;
static module_soil_moisture_values_t  s_val;

void module_soil_moisture_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_val, 0, sizeof(s_val));
}

void module_soil_moisture_tick_100ms(void)
{
}

void module_soil_moisture_tick_1s(void)
{
    s_val.soil_moisture_vwc = 0.0f;
    s_val.quality = 1U;
}

uint8_t module_soil_moisture_apply_config(const module_soil_moisture_config_t *cfg)
{
    if (!cfg) {
        return 1U;
    }
    s_cfg = *cfg;
    return 0U;
}

uint8_t module_soil_moisture_query_state(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_soil_moisture_query_values(void *out)
{
    if (!out) {
        return 1U;
    }
    *(module_soil_moisture_values_t *)out = s_val;
    return 0U;
}

uint8_t module_soil_moisture_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    (void)action_code;
    (void)target_ref;
    (void)payload;
    return 1U;
}

static const module_ops_t s_ops = {
    .module_code    = "soil_moisture_acquisition",
    .init           = module_soil_moisture_init,
    .tick_100ms     = module_soil_moisture_tick_100ms,
    .tick_1s        = module_soil_moisture_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_soil_moisture_apply_config,
    .query_state    = module_soil_moisture_query_state,
    .query_values   = module_soil_moisture_query_values,
    .execute_action = module_soil_moisture_execute_action,
};

const module_ops_t *module_soil_moisture_ops(void)
{
    return &s_ops;
}
