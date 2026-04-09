#include "app_main.h"
#include "board_hw_config.h"
#include "bsp_system.h"
#include "bsp_uart.h"

#include <stdint.h>

int main(void)
{
    bsp_uart_debug_init();
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    bsp_debug_log("[CLK] LAO_CAO: HSI 8MHz (Hardware/3.0 ref), UART BRR 115200=0x45\r\n");
#endif
    /* Debug: UART5 PC12/PD2，8N1，波特率见 board_hw_config.h BOARD_UART_DEBUG_BAUD */
    bsp_debug_log("\r\n\r\n=== APP main() entry ===\r\n");

    {
        uint32_t vtor = *(volatile uint32_t *)0xE000ED08U;
        uint32_t sp   = 0U;
        __asm volatile("mov %0, sp" : "=r"(sp));
        bsp_debug_hex32("VTOR", vtor);
        bsp_debug_hex32("SP  ", sp);
    }

    bsp_debug_log("[APP] calling app_main_init...\r\n");
    app_main_init();
    bsp_debug_log("[APP] app_main_init OK, entering loop\r\n");

    uint32_t ms = 0U;
    for (;;) {
        app_main_loop_iteration(ms);
        if (ms == 0U) {
            bsp_debug_log("[APP] loop tick 0\r\n");
        }
        if (ms == 1000U) {
            bsp_debug_log("[APP] loop tick 1000ms alive\r\n");
        }
        ms += 10U;
        bsp_system_delay_ms(10U);
    }
}
