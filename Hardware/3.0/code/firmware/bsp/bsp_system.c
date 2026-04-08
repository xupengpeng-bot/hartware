#include "bsp_system.h"

void bsp_system_delay_ms(uint32_t ms)
{
    volatile uint32_t n = ms * 800U;
    while (n > 0U) {
        n--;
    }
}
