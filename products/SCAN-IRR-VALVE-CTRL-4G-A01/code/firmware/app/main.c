#include "app_main.h"
#include "boot_diag.h"
#include "board_hw_config.h"
#include "bsp_system.h"
#include "bsp_uart.h"
#include "scan_trial_defs.h"

#include <stdio.h>
#include <stdint.h>

#if defined(BOARD_STM32F103)
#define APP_RCC_BASE         0x40021000U
#define APP_RCC_CSR          (*((volatile uint32_t *)(APP_RCC_BASE + 0x24U)))
#define APP_RCC_CSR_RMVF     (1U << 24)
#define APP_RCC_CSR_PINRSTF  (1U << 26)
#define APP_RCC_CSR_PORRSTF  (1U << 27)
#define APP_RCC_CSR_SFTRSTF  (1U << 28)
#define APP_RCC_CSR_IWDGRSTF (1U << 29)
#define APP_RCC_CSR_WWDGRSTF (1U << 30)
#define APP_RCC_CSR_LPWRRSTF (1U << 31)

static void app_log_reset_cause(void)
{
    uint32_t csr = APP_RCC_CSR;
    char line[192];
    size_t used;
    uint8_t first = 1U;

    boot_diag_capture_reset_csr(csr);

    used = (size_t)snprintf(line, sizeof(line), "[BOOT] reset_csr=0x%08lX cause=",
                            (unsigned long)csr);
    if ((csr & (APP_RCC_CSR_PINRSTF | APP_RCC_CSR_PORRSTF | APP_RCC_CSR_SFTRSTF |
                APP_RCC_CSR_IWDGRSTF | APP_RCC_CSR_WWDGRSTF | APP_RCC_CSR_LPWRRSTF)) == 0U) {
        used += (size_t)snprintf(line + used, sizeof(line) - used, "unknown");
    } else {
        if ((csr & APP_RCC_CSR_PINRSTF) != 0U) {
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%sPIN", first ? "" : "+");
            first = 0U;
        }
        if ((csr & APP_RCC_CSR_PORRSTF) != 0U) {
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%sPOR", first ? "" : "+");
            first = 0U;
        }
        if ((csr & APP_RCC_CSR_SFTRSTF) != 0U) {
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%sSOFT", first ? "" : "+");
            first = 0U;
        }
        if ((csr & APP_RCC_CSR_IWDGRSTF) != 0U) {
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%sIWDG", first ? "" : "+");
            first = 0U;
        }
        if ((csr & APP_RCC_CSR_WWDGRSTF) != 0U) {
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%sWWDG", first ? "" : "+");
            first = 0U;
        }
        if ((csr & APP_RCC_CSR_LPWRRSTF) != 0U) {
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%sLPWR", first ? "" : "+");
        }
    }
    (void)snprintf(line + used, sizeof(line) - used, "\r\n");
    bsp_debug_log(line);
    APP_RCC_CSR |= APP_RCC_CSR_RMVF;
}
#else
static void app_log_reset_cause(void) {}
#endif

int main(void)
{
    bsp_uart_debug_init();
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    bsp_debug_log("[CLK] LAO_CAO: HSI 8MHz (Hardware/SCAN-IRR-VALVE-CTRL-4G-A01 ref), UART BRR 115200=0x45\r\n");
#endif
    /* Debug: UART5 PC12/PD2，8N1，波特率见 board_hw_config.h BOARD_UART_DEBUG_BAUD */
    bsp_debug_log("\r\n\r\n=== APP main() entry ===\r\n");
    {
        char version_line[96];
        (void)snprintf(version_line, sizeof(version_line),
                       "[BOOT] firmware_version=%s\r\n",
                       SCAN_TRIAL_SOFTWARE_VERSION);
        bsp_debug_log(version_line);
    }
    app_log_reset_cause();

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
        if (ms != 0U && (ms % 5000U) == 0U) {
            bsp_debug_log("[APP] loop 5s alive\r\n");
        }
        ms += 10U;
        bsp_system_delay_ms(10U);
    }
}
