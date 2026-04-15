#include "module_single_valve.h"

#include "board_hw_config.h"
#include "bsp_gpio.h"
#include "config_store.h"

#include <string.h>

#define MODULE_VALVE_COUNT 2U
#define MODULE_VALVE_DEFAULT_OPEN_PULSE_MS  300U
#define MODULE_VALVE_DEFAULT_HOLD_PWM_PERIOD_MS 300U
#define MODULE_VALVE_DEFAULT_HOLD_PWM_ON_MS     200U

typedef struct {
    uint16_t             pulse_remaining_ticks;
    uint16_t             pwm_on_ticks;
    uint16_t             pwm_period_ticks;
    uint16_t             pwm_phase_ticks;
    module_valve_state_t final_state;
    module_valve_state_t state;
    uint8_t              hold_pwm_active;
} valve_slot_t;

static valve_slot_t s_valves[MODULE_VALVE_COUNT];
static module_single_valve_config_t s_cfg;

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

static int valve_index_from_target(const char *target_ref)
{
    if (target_ref == NULL || target_ref[0] == '\0' || str_eq(target_ref, "valve_1")) {
        return 0;
    }
    if (str_eq(target_ref, "valve_2")) {
        return 1;
    }
    return -1;
}

static int valve_open_pin(int index)
{
    switch (index) {
    case 0:
        return BSP_GPIO_PORT_B(BOARD_HW_PIN_VALVE1_IN1_PORT_B);
    case 1:
        return BSP_GPIO_PORT_C(BOARD_HW_PIN_VALVE2_IN1_PORT_C);
    default:
        return BSP_GPIO_NONE;
    }
}

static int valve_close_pin(int index)
{
    switch (index) {
    case 0:
        return BSP_GPIO_PORT_B(BOARD_HW_PIN_VALVE1_IN2_PORT_B);
    case 1:
        return BSP_GPIO_PORT_C(BOARD_HW_PIN_VALVE2_IN2_PORT_C);
    default:
        return BSP_GPIO_NONE;
    }
}

static void valve_apply_levels(int index, int open_level, int close_level)
{
    int pin_open = valve_open_pin(index);
    int pin_close = valve_close_pin(index);

    if (pin_open == BSP_GPIO_NONE || pin_close == BSP_GPIO_NONE) {
        return;
    }
    bsp_gpio_config_output(pin_open, open_level);
    bsp_gpio_config_output(pin_close, close_level);
    bsp_gpio_set(pin_open, open_level);
    bsp_gpio_set(pin_close, close_level);
}

static void valve_release_coil(int index)
{
    valve_apply_levels(index, 0, 0);
}

static void valve_drive_open_polarity(int index)
{
    valve_apply_levels(index, 1, 0);
}

static uint16_t valve_ticks_from_ms(uint16_t duration_ms)
{
    uint16_t ticks = (uint16_t)((duration_ms + 99U) / 100U);

    return ticks != 0U ? ticks : 1U;
}

static valve_control_mode_t valve_control_mode_active(void)
{
    const control_config_t *control = config_store_control();

    return control != NULL ? control->valve_control_mode : VALVE_CONTROL_PULSE_OUTPUT;
}

static uint16_t valve_default_open_pulse_ms(void)
{
    const control_config_t *control = config_store_control();

    if (control != NULL && control->valve_open_pulse_ms != 0U) {
        return control->valve_open_pulse_ms;
    }
    return MODULE_VALVE_DEFAULT_OPEN_PULSE_MS;
}

static uint16_t valve_open_pulse_ms(void)
{
    return s_cfg.open_pulse_ms != 0U ? s_cfg.open_pulse_ms : valve_default_open_pulse_ms();
}

static uint16_t valve_hold_pwm_period_ticks(void)
{
    return valve_ticks_from_ms(MODULE_VALVE_DEFAULT_HOLD_PWM_PERIOD_MS);
}

static uint16_t valve_hold_pwm_on_ticks(uint16_t period_ticks)
{
    uint16_t on_ticks = valve_ticks_from_ms(MODULE_VALVE_DEFAULT_HOLD_PWM_ON_MS);

    if (on_ticks > period_ticks) {
        on_ticks = period_ticks;
    }
    return on_ticks;
}

static void valve_stop_hold_pwm(int index)
{
    s_valves[index].hold_pwm_active = 0U;
    s_valves[index].pwm_phase_ticks = 0U;
    s_valves[index].pwm_period_ticks = 0U;
    s_valves[index].pwm_on_ticks = 0U;
}

static void valve_apply_hold_pwm(int index)
{
    if (s_valves[index].hold_pwm_active == 0U || s_valves[index].pwm_period_ticks == 0U) {
        valve_release_coil(index);
        return;
    }
    if (s_valves[index].pwm_on_ticks >= s_valves[index].pwm_period_ticks ||
        s_valves[index].pwm_phase_ticks < s_valves[index].pwm_on_ticks) {
        valve_drive_open_polarity(index);
    } else {
        valve_release_coil(index);
    }
}

static void valve_begin_hold_pwm(int index)
{
    s_valves[index].hold_pwm_active = 1U;
    s_valves[index].pwm_period_ticks = valve_hold_pwm_period_ticks();
    s_valves[index].pwm_on_ticks = valve_hold_pwm_on_ticks(s_valves[index].pwm_period_ticks);
    s_valves[index].pwm_phase_ticks = 0U;
    valve_apply_hold_pwm(index);
}

static void valve_start_pulse(int index, module_valve_state_t active_state,
                              module_valve_state_t final_state, uint16_t pulse_ms)
{
    s_valves[index].state = active_state;
    s_valves[index].final_state = final_state;
    s_valves[index].pulse_remaining_ticks = valve_ticks_from_ms(pulse_ms);
}

static uint8_t valve_start_open(int index)
{
    if (index < 0 || index >= (int)MODULE_VALVE_COUNT) {
        return 1U;
    }
    valve_stop_hold_pwm(index);
    if (valve_control_mode_active() == VALVE_CONTROL_PULSE_OUTPUT) {
        valve_drive_open_polarity(index);
        valve_start_pulse(index, VALVE_OPENING, VALVE_OPEN, valve_open_pulse_ms());
        s_valves[index].hold_pwm_active = 1U;
        return 0U;
    }
    valve_drive_open_polarity(index);
    s_valves[index].pulse_remaining_ticks = 0U;
    s_valves[index].final_state = VALVE_OPEN;
    s_valves[index].state = VALVE_OPEN;
    return 0U;
}

static uint8_t valve_start_close(int index)
{
    if (index < 0 || index >= (int)MODULE_VALVE_COUNT) {
        return 1U;
    }
    valve_stop_hold_pwm(index);
    valve_release_coil(index);
    s_valves[index].pulse_remaining_ticks = 0U;
    s_valves[index].final_state = VALVE_CLOSED;
    s_valves[index].state = VALVE_CLOSED;
    return 0U;
}

void module_single_valve_init(void)
{
    uint8_t i;

    memset(s_valves, 0, sizeof(s_valves));
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.open_pulse_ms = valve_default_open_pulse_ms();

    for (i = 0U; i < MODULE_VALVE_COUNT; i++) {
        s_valves[i].state = VALVE_CLOSED;
        s_valves[i].final_state = VALVE_CLOSED;
        valve_release_coil((int)i);
    }
}

void module_single_valve_tick_100ms(void)
{
    uint8_t i;

    for (i = 0U; i < MODULE_VALVE_COUNT; i++) {
        if (s_valves[i].pulse_remaining_ticks == 0U) {
            if (s_valves[i].hold_pwm_active != 0U && s_valves[i].state == VALVE_OPEN) {
                if (s_valves[i].pwm_period_ticks != 0U) {
                    s_valves[i].pwm_phase_ticks++;
                    if (s_valves[i].pwm_phase_ticks >= s_valves[i].pwm_period_ticks) {
                        s_valves[i].pwm_phase_ticks = 0U;
                    }
                }
                valve_apply_hold_pwm((int)i);
            }
            continue;
        }
        s_valves[i].pulse_remaining_ticks--;
        if (s_valves[i].pulse_remaining_ticks == 0U) {
            s_valves[i].state = s_valves[i].final_state;
            if (s_valves[i].final_state == VALVE_OPEN && s_valves[i].hold_pwm_active != 0U) {
                valve_begin_hold_pwm((int)i);
            } else {
                valve_release_coil((int)i);
            }
        }
    }
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
    *(module_valve_state_t *)out = s_valves[0].state;
    return 0U;
}

int module_single_valve_query_state_by_target(const char *target_ref, module_valve_state_t *out_state)
{
    int index = valve_index_from_target(target_ref);

    if (index < 0 || out_state == NULL) {
        return -1;
    }
    *out_state = s_valves[index].state;
    return 0;
}

uint8_t module_single_valve_query_values(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_single_valve_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    int index;

    (void)payload;

    index = valve_index_from_target(target_ref);
    if (index < 0) {
        return 1U;
    }
    if (str_eq(action_code, "open_valve")) {
        return valve_start_open(index);
    }
    if (str_eq(action_code, "close_valve")) {
        return valve_start_close(index);
    }
    return 1U;
}

static const module_ops_t s_dual_ops = {
    .module_code    = "dual_valve_control",
    .init           = module_single_valve_init,
    .tick_100ms     = module_single_valve_tick_100ms,
    .tick_1s        = module_single_valve_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_single_valve_apply_config,
    .query_state    = module_single_valve_query_state,
    .query_values   = module_single_valve_query_values,
    .execute_action = module_single_valve_execute_action,
};

const module_ops_t *module_dual_valve_ops(void)
{
    return &s_dual_ops;
}
