/*
 * VTOR 必须与链接脚本中 .isr_vector 起始地址一致。
 * - 默认 0x08004000：前有 bootloader@0x08000000；
 * - 独立固件编译时定义 APP_FLASH_VECTOR_ADDR=0x08000000U。
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
    /* 时钟：LAO_CAO=HSI 8MHz；否则 HSE+PLL 72MHz。PB4 需 AFIO 关 JTAG */
    board_clock_init();
    /* 尽早关闭 JTAG、保留 SWD，PB4 才能作 NET_PWRKEY GPIO（否则为 NJTRST） */
    RCC_APB2ENR_STM |= (1U << 0); /* AFIOEN */
    {
        uint32_t v = AFIO_MAPR_STM;
        v &= ~(7U << 24);
        v |= (2U << 24);
        AFIO_MAPR_STM = v;
    }
#endif
}
