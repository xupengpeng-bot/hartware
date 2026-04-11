#include "module_pressure.h"
#include <string.h>

static module_pressure_config_t  s_cfg;
static module_pressure_values_t  s_val;

void module_pressure_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_val, 0, sizeof(s_val));
    s_cfg.sample_interval_ms = 1000U;
    s_cfg.full_scale = 1.0f;
}

void module_pressure_tick_100ms(void)
{
}

void module_pressure_tick_1s(void)
{
    /* Real pressure source not wired yet: report unavailable instead of a fake fixed value. */
    s_val.pressure_mpa = 0.0f;
    s_val.quality = 0U;
}

uint8_t module_pressure_apply_config(const module_pressure_config_t *cfg)
{
    if (!cfg) {
        return 1U;
    }
    s_cfg = *cfg;
    return 0U;
}

uint8_t module_pressure_query_state(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_pressure_query_values(void *out)
{
    if (!out) {
        return 1U;
    }
    *(module_pressure_values_t *)out = s_val;
    return 0U;
}

uint8_t module_pressure_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    (void)action_code;
    (void)target_ref;
    (void)payload;
    return 1U;
}

static void ops_init(void)
{
    module_pressure_init();
}
static void ops_tick100(void)
{
    module_pressure_tick_100ms();
}
static void ops_tick1s(void)
{
    module_pressure_tick_1s();
}
static uint8_t ops_apply(const void *cfg)
{
    return module_pressure_apply_config((const module_pressure_config_t *)cfg);
}
static uint8_t ops_qs(void *out)
{
    return module_pressure_query_state(out);
}
static uint8_t ops_qv(void *out)
{
    return module_pressure_query_values(out);
}
static uint8_t ops_ex(const char *a, const char *t, const void *p)
{
    return module_pressure_execute_action(a, t, p);
}

static const module_ops_t s_ops = {
    .module_code     = "pressure_acquisition",
    .init            = ops_init,
    .tick_100ms      = ops_tick100,
    .tick_1s         = ops_tick1s,
    .apply_config    = ops_apply,
    .query_state     = ops_qs,
    .query_values    = ops_qv,
    .execute_action  = ops_ex,
};

const module_ops_t *module_pressure_ops(void)
{
    return &s_ops;
}

int module_pressure_get_mpa(float *out)
{
    if (!out) {
        return -1;
    }
    *out = s_val.pressure_mpa;
    return 0;
}
