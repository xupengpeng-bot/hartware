#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

#define BSP_GPIO_NONE          (-1)
#define BSP_GPIO_PORT_A(pin)   ((int)(0x00 | ((pin) & 0x0F)))
#define BSP_GPIO_PORT_B(pin)   ((int)(0x10 | ((pin) & 0x0F)))
#define BSP_GPIO_PORT_C(pin)   ((int)(0x20 | ((pin) & 0x0F)))
#define BSP_GPIO_PORT_D(pin)   ((int)(0x30 | ((pin) & 0x0F)))

void bsp_gpio_config_output(int pin, int initial_level);
void bsp_gpio_set(int pin, int level);
int  bsp_gpio_get(int pin);

#endif /* BSP_GPIO_H */
