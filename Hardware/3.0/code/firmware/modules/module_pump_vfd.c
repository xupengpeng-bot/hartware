#include "module_pump_vfd.h"
#include <string.h>

static module_pump_vfd_config_t s_cfg;
static module_pump_vfd_state_t  s_st;
static float                    s_freq_hz;

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

void module_pump_vfd_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_st     = PUMP_VFD_IDLE;
    s_freq_hz = 0.0f;
    s_cfg.start_timeout_ms = 5000U;
    s_cfg.stop_timeout_ms = 5000U;
}

void module_pump_vfd_tick_100ms(void)
{
}

void module_pump_vfd_tick_1s(void)
{
}

uint8_t module_pump_vfd_apply_config(const module_pump_vfd_config_t *cfg)
{
    if (!cfg) {
        return 1U;
    }
    s_cfg = *cfg;
    return 0U;
}

uint8_t module_pump_vfd_query_state(void *out)
{
    if (!out) {
        return 1U;
    }
    *(module_pump_vfd_state_t *)out = s_st;
    return 0U;
}

uint8_t module_pump_vfd_query_values(void *out)
{
    if (!out) {
        return 1U;
    }
    *(float *)out = s_freq_hz;
    return 0U;
}

uint8_t module_pump_vfd_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    (void)target_ref;
    (void)payload;
    if (str_eq(action_code, "start_vfd")) {
        s_st = PUMP_VFD_RUNNING;
        return 0U;
    }
    if (str_eq(action_code, "stop_vfd")) {
        s_st = PUMP_VFD_IDLE;
        s_freq_hz = 0.0f;
        return 0U;
    }
    if (str_eq(action_code, "set_frequency")) {
        if (payload) {
            s_freq_hz = *(const float *)payload;
        }
        return 0U;
    }
    return 1U;
}

static const module_ops_t s_ops = {
    .module_code    = "pump_vfd_control",
    .init           = module_pump_vfd_init,
    .tick_100ms     = module_pump_vfd_tick_100ms,
    .tick_1s        = module_pump_vfd_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_pump_vfd_apply_config,
    .query_state    = module_pump_vfd_query_state,
    .query_values   = module_pump_vfd_query_values,
    .execute_action = module_pump_vfd_execute_action,
};

const module_ops_t *module_pump_vfd_ops(void)
{
    return &s_ops;
}

int module_pump_vfd_query_state_u8(uint8_t *out_state)
{
    if (!out_state) {
        return -1;
    }
    *out_state = (uint8_t)s_st;
    return 0;
}
