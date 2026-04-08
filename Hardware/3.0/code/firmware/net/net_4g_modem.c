#include "net_4g_modem.h"
#include "board_hw_config.h"
#include "bsp_system.h"
#include "bsp_uart.h"

#include <string.h>

/* EC801 is wired to UART4 (PC10/PC11) and NET_PWRKEY on PB5. */

#if defined(BOARD_STM32F103)

#define RCC_BASE           0x40021000U
#define GPIOB_BASE         0x40010C00U

#define RCC_APB2ENR        (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define GPIOB_CRL          (*((volatile uint32_t *)(GPIOB_BASE + 0x00U)))
#define GPIOB_BSRR         (*((volatile uint32_t *)(GPIOB_BASE + 0x10U)))

#define RCC_APB2ENR_IOPBEN (1U << 3)

static void modem_pwrkey_gpio_init(void)
{
    uint32_t crl;

    RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;

    crl = GPIOB_CRL;
    crl &= ~(0xFU << 20U);
    crl |= (0x3U << 20U);
    GPIOB_CRL = crl;

    GPIOB_BSRR = (1U << (BOARD_HW_PIN_NET_PWRKEY_PORT_B + 16U));
}

static void modem_pwrkey_set(int level)
{
    if (level != 0) {
        GPIOB_BSRR = (1U << BOARD_HW_PIN_NET_PWRKEY_PORT_B);
    } else {
        GPIOB_BSRR = (1U << (BOARD_HW_PIN_NET_PWRKEY_PORT_B + 16U));
    }
}

#else

static void modem_pwrkey_gpio_init(void) {}

static void modem_pwrkey_set(int level)
{
    (void)level;
}

#endif

static int s_online;

static void modem_drain_rx(void)
{
    uint8_t ch;

    while (bsp_uart_read((int)BOARD_HW_UART_PORT_MODEM_4G, &ch, 1U) > 0) {
    }
}

static int modem_wait_for_ready(uint32_t timeout_ms)
{
    char     window[32];
    size_t   wlen = 0U;
    uint8_t  ch;
    uint32_t waited = 0U;

    memset(window, 0, sizeof(window));

    while (waited < timeout_ms) {
        if (bsp_uart_read((int)BOARD_HW_UART_PORT_MODEM_4G, &ch, 1U) > 0) {
            if (wlen + 1U >= sizeof(window)) {
                memmove(window, window + 1, sizeof(window) - 2U);
                wlen = sizeof(window) - 2U;
            }
            window[wlen++] = (char)ch;
            window[wlen] = '\0';

            if (strstr(window, "OK") != NULL || strstr(window, "RDY") != NULL || strstr(window, "READY") != NULL) {
                return 1;
            }
            continue;
        }

        bsp_system_delay_ms(20U);
        waited += 20U;
    }

    return 0;
}

static int modem_probe_online(void)
{
    static const char at_cmd[] = "AT\r\n";

    for (int attempt = 0; attempt < 5; ++attempt) {
        modem_drain_rx();
        (void)bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, (const uint8_t *)at_cmd, sizeof(at_cmd) - 1U);
        if (modem_wait_for_ready(1000U) != 0) {
            return 1;
        }
        bsp_system_delay_ms(500U);
    }

    return 0;
}

void net_4g_modem_init(void)
{
    s_online = 0;

    bsp_debug_log("[4G] init start\r\n");
    bsp_uart_modem_init();
    modem_pwrkey_gpio_init();
    modem_drain_rx();

    /*
     * The schematic drives the module PWRKEY pin through an NMOS. Driving the
     * MCU NET_PWRKEY signal high should therefore pull the module PWRKEY low
     * for the required power-on pulse.
     */
    modem_pwrkey_set(1);
    bsp_system_delay_ms(1200U);
    modem_pwrkey_set(0);

    bsp_debug_log("[4G] PWRKEY pulse sent, waiting for boot\r\n");
    bsp_system_delay_ms(5000U);

    if (modem_probe_online() != 0) {
        s_online = 1;
        bsp_debug_log("[4G] modem responded to AT\r\n");
    } else {
        bsp_debug_log("[4G] modem did not respond to AT\r\n");
    }
}

bool net_4g_modem_is_online(void)
{
    return s_online != 0;
}

void net_4g_modem_poll(void)
{
}
