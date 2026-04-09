#include "bsp_status_led.h"
#include "board_hw_config.h"
#include "common_status.h"

#if defined(BOARD_STM32F103)

#define RCC_BASE    0x40021000U
#define GPIOC_BASE  0x40011000U
#define RCC_APB2ENR (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define GPIOC_CRL   (*((volatile uint32_t *)(GPIOC_BASE + 0x00U)))
#define GPIOC_BSRR  (*((volatile uint32_t *)(GPIOC_BASE + 0x10U)))

#define RCC_APB2ENR_IOPCEN (1U << 4)

static void led_pin_out(unsigned pin, int level)
{
    if (level != 0) {
        GPIOC_BSRR = (1U << pin);
    } else {
        GPIOC_BSRR = (1U << (pin + 16U));
    }
}

static void gpio_pc_pin_init(unsigned pin)
{
    uint32_t crl = GPIOC_CRL;
    crl &= ~(0xFU << (pin * 4U));
    crl |= (0x2U << (pin * 4U));
    GPIOC_CRL = crl;
    led_pin_out(pin, 0);
}

void bsp_status_led_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;
    gpio_pc_pin_init(BOARD_HW_PIN_STATUS_LED_A_PORT_C);
    if (BOARD_HW_PIN_STATUS_LED_B_PORT_C != BOARD_HW_PIN_STATUS_LED_NONE) {
        gpio_pc_pin_init(BOARD_HW_PIN_STATUS_LED_B_PORT_C);
    }
}

void bsp_status_led_poll(uint32_t monotonic_ms)
{
    const common_status_t *st = common_status_get();
    unsigned                 pin_a = BOARD_HW_PIN_STATUS_LED_A_PORT_C;
    unsigned                 pin_b = BOARD_HW_PIN_STATUS_LED_B_PORT_C;

    if (!st->online) {
        led_pin_out(pin_a, ((monotonic_ms / 500U) % 2U) != 0U ? 1 : 0);
    } else if (!st->tcp_connected) {
        led_pin_out(pin_a, ((monotonic_ms / 125U) % 2U) != 0U ? 1 : 0);
    } else {
        led_pin_out(pin_a, 1);
    }

    if (pin_b != BOARD_HW_PIN_STATUS_LED_NONE) {
        if (!st->registered_once) {
            led_pin_out(pin_b, 0);
        } else if (monotonic_ms < st->heartbeat_led_pulse_until_ms) {
            led_pin_out(pin_b, 0);
        } else {
            led_pin_out(pin_b, 1);
        }
    }
}

#else

void bsp_status_led_init(void)
{
}

void bsp_status_led_poll(uint32_t monotonic_ms)
{
    (void)monotonic_ms;
}

#endif
