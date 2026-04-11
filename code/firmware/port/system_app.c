/*
 * VTOR must match the linked application vector address.
 * - Default 0x08004000: bootloader + OTA metadata/control occupy the first 16KB.
 * - Standalone firmware can override with APP_FLASH_VECTOR_ADDR=0x08000000U.
 */
#define SCB_VTOR_ADDR 0xE000ED08U
#ifndef APP_FLASH_VECTOR_ADDR
#define APP_FLASH_VECTOR_ADDR 0x08004000U
#endif

#if defined(BOARD_STM32F103)
#include "board_clock_stm32f103.h"
#endif

#if defined(BOARD_STM32F103)
#define RCC_BASE_STM 0x40021000U
#define RCC_APB2ENR_STM (*((volatile uint32_t *)(RCC_BASE_STM + 0x18U)))
#define AFIO_BASE_STM 0x40010000U
#define AFIO_MAPR_STM (*((volatile uint32_t *)(AFIO_BASE_STM + 0x04U)))
#endif

void SystemInit(void)
{
    *(volatile uint32_t *)SCB_VTOR_ADDR = APP_FLASH_VECTOR_ADDR;
#if defined(BOARD_STM32F103)
    /* Clock source: legacy board can stay on HSI, otherwise HSE+PLL 72MHz. */
    board_clock_init();
    /* Disable JTAG but keep SWD so PB4 can be reused as NET_PWRKEY. */
    RCC_APB2ENR_STM |= (1U << 0); /* AFIOEN */
    {
        uint32_t v = AFIO_MAPR_STM;
        v &= ~(7U << 24);
        v |= (2U << 24);
        AFIO_MAPR_STM = v;
    }
#endif
}
