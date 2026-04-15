#include "bsp_system.h"
#if defined(BOARD_STM32F103)
#include "board_clock_stm32f103.h"
#endif

void bsp_system_delay_ms(uint32_t ms)
{
    volatile uint32_t n;
#if defined(BOARD_STM32F103)
    /* ~10 cycles/iteration @ 8MHz 时等价于旧 ms*800；PLL 到 72MHz 后仍校准为毫秒 */
    n = (ms * SystemCoreClock) / 10000U;
    if (n == 0U && ms != 0U) {
        n = 1U;
    }
#else
    n = ms * 800U;
#endif
    while (n > 0U) {
        n--;
    }
}
