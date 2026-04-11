#include "module_pump_vfd.h"

#include "board_hw_config.h"
#include "bsp_gpio.h"
#include "config_store.h"

#include <string.h>

static module_pump_vfd_config_t s_cfg;
static module_pump_vfd_state_t  s_st;
static float                    s_freq_hz;

static int pump_local_output_pin(void)
{
    const device_config_t *cfg = config_store_active();
    uint32_t i;

    if (cfg == NULL) {
        return BSP_GPIO_NONE;
    }
    for (i = 0U; i < cfg->channel_binding_count; i++) {
        const channel_binding_t *binding = &cfg->channel_bindings[i];
        if (binding->enabled == 0U) {
            continue;
        }
        if (strcmp(binding->channel_role, "pump_run") != 0) {
            continue;
        }
        if (strcmp(binding->resource_ref, "relay_pump_run") == 0) {
            return BSP_GPIO_PORT_C(BOARD_HW_PIN_PUMP_RUN_PORT_C);
        }
    }
    return BSP_GPIO_NONE;
}

static int pump_local_active_level(void)
{
    return BOARD_HW_PUMP_RUN_ACTIVE_LEVEL != 0U ? 1 : 0;
}

static uint8_t pump_supports_local_drive(void)
{
    const control_config_t *control = config_store_control();
    pump_control_mode_t mode = PUMP_CONTROL_RELAY_DIRECT;

    if (control != NULL) {
        mode = control->pump_control_mode;
    }
    return (mode == PUMP_CONTROL_RELAY_DIRECT || mode == PUMP_CONTROL_CONTACTOR_DIRECT) ? 1U : 0U;
}

static uint8_t pump_drive_output(uint8_t on)
{
    int pin = pump_local_output_pin();
    int active = pump_local_active_level();

    if (pump_supports_local_drive() == 0U || pin == BSP_GPIO_NONE) {
        return 1U;
    }

    bsp_gpio_config_output(pin, on != 0U ? active : !active);
    bsp_gpio_set(pin, on != 0U ? active : !active);
    return 0U;
}

static uint8_t pump_output_is_on(void)
{
    int pin = pump_local_output_pin();
    int active = pump_local_active_level();

    if (pump_supports_local_drive() == 0U || pin == BSP_GPIO_NONE) {
        return s_st == PUMP_VFD_RUNNING ? 1U : 0U;
    }
    return bsp_gpio_get(pin) == active ? 1U : 0U;
}

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
    if (pump_supports_local_drive() != 0U) {
        int pin = pump_local_output_pin();
        int inactive = pump_local_active_level() ? 0 : 1;
        if (pin != BSP_GPIO_NONE) {
            bsp_gpio_config_output(pin, inactive);
        }
    }
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
    *(module_pump_vfd_state_t *)out = pump_output_is_on() != 0U ? PUMP_VFD_RUNNING : PUMP_VFD_IDLE;
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
        if (pump_drive_output(1U) != 0U) {
            return 1U;
        }
        s_st = PUMP_VFD_RUNNING;
        return 0U;
    }
    if (str_eq(action_code, "stop_vfd")) {
        if (pump_drive_output(0U) != 0U) {
            return 1U;
        }
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
    *out_state = (uint8_t)(pump_output_is_on() != 0U ? PUMP_VFD_RUNNING : PUMP_VFD_IDLE);
    return 0;
}
