#include "module_single_valve.h"
#include <string.h>

static module_single_valve_config_t s_cfg;
static module_valve_state_t         s_st;

static int str_eq(const char *a, const char *b)
{
    if (!a || !b) {
        return 0;
    }
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

void module_single_valve_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_st = VALVE_CLOSED;
    s_cfg.action_timeout_ms = 30000U;
}

void module_single_valve_tick_100ms(void)
{
}

void module_single_valve_tick_1s(void)
{
}

uint8_t module_single_valve_apply_config(const module_single_valve_config_t *cfg)
{
    if (!cfg) {
        return 1U;
    }
    s_cfg = *cfg;
    return 0U;
}

uint8_t module_single_valve_query_state(void *out)
{
    if (!out) {
        return 1U;
    }
    *(module_valve_state_t *)out = s_st;
    return 0U;
}

uint8_t module_single_valve_query_values(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_single_valve_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    (void)target_ref;
    (void)payload;
    if (str_eq(action_code, "open_valve")) {
        s_st = VALVE_OPEN;
        return 0U;
    }
    if (str_eq(action_code, "close_valve")) {
        s_st = VALVE_CLOSED;
        return 0U;
    }
    return 1U;
}

static const module_ops_t s_ops = {
    .module_code    = "single_valve_control",
    .init           = module_single_valve_init,
    .tick_100ms     = module_single_valve_tick_100ms,
    .tick_1s        = module_single_valve_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_single_valve_apply_config,
    .query_state    = module_single_valve_query_state,
    .query_values   = module_single_valve_query_values,
    .execute_action = module_single_valve_execute_action,
};

const module_ops_t *module_single_valve_ops(void)
{
    return &s_ops;
}
