/*
 * Minimal second-stage bootloader for STM32F103RC (256K flash class).
 * Lives at 0x08000000, jumps to application vector table at 0x08010000.
 * Flashing this image + APP is done via USART ROM bootloader (stm32flash) or ST-Link.
 */
#include <stdint.h>

#define APP_VECTORS 0x08010000u
#define SCB_VTOR    (*(volatile uint32_t *)0xE000ED08u)

static uint32_t rd32(uint32_t a)
{
    return *(volatile uint32_t *)a;
}

static int app_ok(void)
{
    uint32_t sp = rd32(APP_VECTORS);
    uint32_t pc = rd32(APP_VECTORS + 4u);

    /* Initial MSP in internal SRAM (48K @ 0x20000000 on F103xC) */
    if ((sp & 0xFFFE0000u) != 0x20000000u) {
        return 0;
    }
    if ((pc & 1u) == 0u) {
        return 0;
    }
    pc &= ~1u;
    if (pc < APP_VECTORS || pc >= (APP_VECTORS + 256u * 1024u)) {
        return 0;
    }
    return 1;
}

static void set_msp(uint32_t sp)
{
    __asm volatile("msr msp, %0" : : "r"(sp) : "memory");
}

static void go_app(void)
{
    uint32_t sp = rd32(APP_VECTORS);
    uint32_t pc = rd32(APP_VECTORS + 4u);
    void (*reset)(void) = (void (*)(void))pc;

    __asm volatile("cpsid i" ::: "memory");
    SCB_VTOR = APP_VECTORS;
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
    set_msp(sp);
    /* Reset leaves interrupts enabled; restore that contract before entering the APP. */
    __asm volatile("cpsie i" ::: "memory");
    reset();
}

int main(void)
{
    if (app_ok()) {
        go_app();
    }

    for (;;) {
        __asm volatile("wfi");
    }
}
