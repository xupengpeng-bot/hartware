#ifndef BOARD_CLOCK_STM32F103_H
#define BOARD_CLOCK_STM32F103_H

#include <stdint.h>

/** LAO_CAO=1：仅 HSI 8MHz（同 Hardware/SCAN-IRR-CTRL-4G-A01）。否则 HSE×9→72MHz，失败则 HSI 8MHz。 */
void board_clock_init(void);

extern uint32_t SystemCoreClock;

#endif
