#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

void bsp_gpio_set(int pin, int level);
int  bsp_gpio_get(int pin);

#endif /* BSP_GPIO_H */
