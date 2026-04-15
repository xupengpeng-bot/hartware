#ifndef VALVE_DRIVER_H
#define VALVE_DRIVER_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * H bridge input mapping.
 * Change these macros if your MCU pins are different.
 */
#define VALVE_GPIO_PORT     GPIOA
#define VALVE_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define VALVE_IN1_GPIO_PORT VALVE_GPIO_PORT
#define VALVE_IN1_GPIO_PIN  GPIO_PIN_0
#define VALVE_IN2_GPIO_PORT VALVE_GPIO_PORT
#define VALVE_IN2_GPIO_PIN  GPIO_PIN_1

/*
 * Pulse width for the latching solenoid.
 * Adjust this value according to the valve datasheet.
 */
#define VALVE_PULSE_WIDTH_MS 30U

/*
 * Short interlock delay before changing direction.
 * This keeps both bridge inputs low briefly and helps avoid shoot-through.
 */
#define VALVE_INTERLOCK_DELAY_MS 1U

void valve_init(void);
void valve_open(void);
void valve_close(void);

#ifdef __cplusplus
}
#endif

#endif /* VALVE_DRIVER_H */
