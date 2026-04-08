#include "module_flow.h"
#include <string.h>

static module_flow_config_t  s_cfg;
static module_flow_values_t  s_val;

void module_flow_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_val, 0, sizeof(s_val));
    s_cfg.k_factor = 1.0f;
    s_cfg.sample_interval_ms = 1000U;
}

void module_flow_tick_100ms(void)
{
}

void module_flow_tick_1s(void)
{
    s_val.instant_m3h = 0.0f;
    s_val.quality = 1U;
}

uint8_t module_flow_apply_config(const module_flow_config_t *cfg)
{
    if (!cfg) {
        return 1U;
    }
    s_cfg = *cfg;
    return 0U;
}

uint8_t module_flow_query_state(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_flow_query_values(void *out)
{
    if (!out) {
        return 1U;
    }
    *(module_flow_values_t *)out = s_val;
    return 0U;
}

uint8_t module_flow_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    (void)action_code;
    (void)target_ref;
    (void)payload;
    return 1U;
}

static const module_ops_t s_ops = {
    .module_code    = "flow_acquisition",
    .init           = module_flow_init,
    .tick_100ms     = module_flow_tick_100ms,
    .tick_1s        = module_flow_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_flow_apply_config,
    .query_state    = module_flow_query_state,
    .query_values   = module_flow_query_values,
    .execute_action = module_flow_execute_action,
};

const module_ops_t *module_flow_ops(void)
{
    return &s_ops;
}

int module_flow_get_instant(float *out_m3h)
{
    if (!out_m3h) {
        return -1;
    }
    *out_m3h = s_val.instant_m3h;
    return 0;
}
