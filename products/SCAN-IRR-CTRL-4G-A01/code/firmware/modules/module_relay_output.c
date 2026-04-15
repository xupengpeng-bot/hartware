#include "module_relay_output.h"

#include "board_hw_config.h"
#include "bsp_gpio.h"

#include <string.h>

#define MODULE_RELAY_OUTPUT_COUNT 1U

static module_relay_output_state_t s_states[MODULE_RELAY_OUTPUT_COUNT];

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

static int relay_index_from_target(const char *target_ref)
{
    if (target_ref == NULL || target_ref[0] == '\0') {
        return -1;
    }
    if (str_eq(target_ref, "relay_1")) {
        return 0;
    }
    return -1;
}

static int relay_pin(int index)
{
    switch (index) {
    case 0:
        return BSP_GPIO_PORT_C(BOARD_HW_PIN_RELAY1_PORT_C);
    case 1:
        return BSP_GPIO_PORT_C(BOARD_HW_PIN_RELAY2_PORT_C);
    default:
        return BSP_GPIO_NONE;
    }
}

static int relay_active_level(void)
{
    return BOARD_HW_PUMP_RUN_ACTIVE_LEVEL != 0U ? 1 : 0;
}

static uint8_t relay_drive_output(int index, uint8_t on)
{
    int pin = relay_pin(index);
    int active = relay_active_level();
    int level = on != 0U ? active : !active;

    if (pin == BSP_GPIO_NONE) {
        return 1U;
    }

    bsp_gpio_config_output(pin, level);
    bsp_gpio_set(pin, level);
    s_states[index] = on != 0U ? RELAY_OUTPUT_ON : RELAY_OUTPUT_OFF;
    return 0U;
}

static module_relay_output_state_t relay_current_state(int index)
{
    int pin = relay_pin(index);
    int active = relay_active_level();

    if (pin == BSP_GPIO_NONE) {
        return RELAY_OUTPUT_OFF;
    }
    return bsp_gpio_get(pin) == active ? RELAY_OUTPUT_ON : RELAY_OUTPUT_OFF;
}

void module_relay_output_init(void)
{
    uint8_t i;

    memset(s_states, 0, sizeof(s_states));
    for (i = 0U; i < MODULE_RELAY_OUTPUT_COUNT; i++) {
        (void)relay_drive_output((int)i, 0U);
    }
}

void module_relay_output_tick_100ms(void)
{
}

void module_relay_output_tick_1s(void)
{
}

uint8_t module_relay_output_apply_config(const void *cfg)
{
    (void)cfg;
    return 0U;
}

uint8_t module_relay_output_query_state(void *out)
{
    if (out == NULL) {
        return 1U;
    }
    *(module_relay_output_state_t *)out = relay_current_state(0);
    return 0U;
}

uint8_t module_relay_output_query_values(void *out)
{
    (void)out;
    return 0U;
}

int module_relay_output_query_state_by_target(const char *target_ref, module_relay_output_state_t *out_state)
{
    int index = relay_index_from_target(target_ref);

    if (index < 0 || out_state == NULL) {
        return -1;
    }
    *out_state = relay_current_state(index);
    return 0;
}

uint8_t module_relay_output_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    int index;

    (void)payload;

    index = relay_index_from_target(target_ref);
    if (index < 0) {
        return 1U;
    }
    if (str_eq(action_code, "open_relay")) {
        return relay_drive_output(index, 1U);
    }
    if (str_eq(action_code, "close_relay")) {
        return relay_drive_output(index, 0U);
    }
    return 1U;
}

static const module_ops_t s_ops = {
    .module_code    = "relay_output_control",
    .init           = module_relay_output_init,
    .tick_100ms     = module_relay_output_tick_100ms,
    .tick_1s        = module_relay_output_tick_1s,
    .apply_config   = module_relay_output_apply_config,
    .query_state    = module_relay_output_query_state,
    .query_values   = module_relay_output_query_values,
    .execute_action = module_relay_output_execute_action,
};

const module_ops_t *module_relay_output_ops(void)
{
    return &s_ops;
}
