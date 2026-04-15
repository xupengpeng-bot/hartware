#include "valve_driver.h"

/*
 * Note for TB6612 users:
 * The STBY pin must be pulled high or driven high before calling these APIs.
 * This example only shows the two direction pins required by the user request.
 */

static void valve_bridge_off(void)
{
    /* Release the H bridge so the solenoid is not energized continuously. */
    HAL_GPIO_WritePin(VALVE_IN1_GPIO_PORT, VALVE_IN1_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VALVE_IN2_GPIO_PORT, VALVE_IN2_GPIO_PIN, GPIO_PIN_RESET);
}

static void valve_send_pulse(GPIO_PinState in1_state, GPIO_PinState in2_state)
{
    /* Always force both inputs low first to avoid bridge cross-conduction. */
    valve_bridge_off();
    HAL_Delay(VALVE_INTERLOCK_DELAY_MS);

    /* Drive one polarity across the latching solenoid coil. */
    HAL_GPIO_WritePin(VALVE_IN1_GPIO_PORT, VALVE_IN1_GPIO_PIN, in1_state);
    HAL_GPIO_WritePin(VALVE_IN2_GPIO_PORT, VALVE_IN2_GPIO_PIN, in2_state);

    /* Hold the pulse for the required latch time. */
    HAL_Delay(VALVE_PULSE_WIDTH_MS);

    /* Return both inputs low immediately after the pulse. */
    valve_bridge_off();
}

void valve_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    /* Enable the GPIO clock for the selected port. */
    VALVE_GPIO_CLK_ENABLE();

    /* Configure both H bridge inputs as push-pull outputs. */
    gpio_init.Pin = VALVE_IN1_GPIO_PIN | VALVE_IN2_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(VALVE_GPIO_PORT, &gpio_init);

    /* Safe default state: both inputs low, bridge output disabled. */
    valve_bridge_off();
}

void valve_open(void)
{
    /*
     * Forward pulse:
     * IN1 = High, IN2 = Low
     * This polarity opens the latching valve.
     */
    valve_send_pulse(GPIO_PIN_SET, GPIO_PIN_RESET);
}

void valve_close(void)
{
    /*
     * Reverse pulse:
     * IN1 = Low, IN2 = High
     * This polarity closes the latching valve.
     */
    valve_send_pulse(GPIO_PIN_RESET, GPIO_PIN_SET);
}

/*
Example usage:

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    valve_init();
    valve_open();       // Open the valve with a 30 ms forward pulse
    HAL_Delay(5000);    // Valve stays open because it is latching
    valve_close();      // Close the valve with a 30 ms reverse pulse

    while (1) {
    }
}
*/
