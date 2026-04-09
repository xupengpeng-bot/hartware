/*
 * - BOARD_CLOCK_LAO_CAO_HSI_8MHZ=1：与 Hardware/3.0 一致，仅 HSI 8MHz，不写 PLL（参考 SystemInit 空实现时的上电默认）。
 * - 否则：HSE ×9 → 72MHz SYSCLK，APB1=/2，USART 内核 72MHz。
 */
#include "board_clock_stm32f103.h"
#include "board_hw_config.h"

#define RCC_BASE     0x40021000U
#define FLASH_R_BASE 0x40022000U

#define RCC_CR       (*((volatile uint32_t *)(RCC_BASE + 0x00U)))
#define RCC_CFGR     (*((volatile uint32_t *)(RCC_BASE + 0x04U)))
#define FLASH_ACR    (*((volatile uint32_t *)(FLASH_R_BASE + 0x00U)))

#define RCC_CR_HSEON  (1U << 16)
#define RCC_CR_HSERDY (1U << 17)
#define RCC_CR_PLLON  (1U << 24)
#define RCC_CR_PLLRDY (1U << 25)

#define FLASH_ACR_LATENCY_2 (2U << 0)
#define FLASH_ACR_PRFTBE    (1U << 4)

#define RCC_CFGR_PLLMULL9   (7U << 18)
#define RCC_CFGR_PLLSRC_HSE (1U << 16)
#define RCC_CFGR_SW_PLL     (2U << 0)

uint32_t SystemCoreClock = 8000000U;

void board_clock_init(void)
{
    uint32_t t;
    uint32_t cfgr;

#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    SystemCoreClock = 8000000U;
    return;
#endif

    RCC_CR |= RCC_CR_HSEON;
    for (t = 0U; t < 0xFFFFU; t++) {
        if ((RCC_CR & RCC_CR_HSERDY) != 0U) {
            break;
        }
    }
    if ((RCC_CR & RCC_CR_HSERDY) == 0U) {
        SystemCoreClock = 8000000U;
        return;
    }

    FLASH_ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    cfgr = RCC_CFGR;
    cfgr &= ~((0xFU << 18) | (1U << 16) | (7U << 8) | (7U << 11) | (0xFU << 4));
    cfgr |= RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC_HSE;
    RCC_CFGR = cfgr;

    RCC_CR |= RCC_CR_PLLON;
    for (t = 0U; t < 0xFFFFU; t++) {
        if ((RCC_CR & RCC_CR_PLLRDY) != 0U) {
            break;
        }
    }
    if ((RCC_CR & RCC_CR_PLLRDY) == 0U) {
        SystemCoreClock = 8000000U;
        return;
    }

    cfgr = RCC_CFGR;
    cfgr &= ~3U;
    cfgr |= RCC_CFGR_SW_PLL;
    RCC_CFGR = cfgr;

    for (t = 0U; t < 0xFFFFU; t++) {
        if (((RCC_CFGR >> 2) & 3U) == 2U) {
            break;
        }
    }

    SystemCoreClock = 72000000U;
}
