#include "bsp_gpio.h"

#if defined(BOARD_STM32F103)

#define RCC_BASE           0x40021000U
#define GPIOA_BASE         0x40010800U
#define GPIOB_BASE         0x40010C00U
#define GPIOC_BASE         0x40011000U
#define GPIOD_BASE         0x40011400U

#define RCC_APB2ENR        (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define RCC_APB2ENR_IOPAEN (1U << 2)
#define RCC_APB2ENR_IOPBEN (1U << 3)
#define RCC_APB2ENR_IOPCEN (1U << 4)
#define RCC_APB2ENR_IOPDEN (1U << 5)

#define GPIO_CRL(base)     (*((volatile uint32_t *)((base) + 0x00U)))
#define GPIO_CRH(base)     (*((volatile uint32_t *)((base) + 0x04U)))
#define GPIO_IDR(base)     (*((volatile uint32_t *)((base) + 0x08U)))
#define GPIO_ODR(base)     (*((volatile uint32_t *)((base) + 0x0CU)))
#define GPIO_BSRR(base)    (*((volatile uint32_t *)((base) + 0x10U)))
#define GPIO_BRR(base)     (*((volatile uint32_t *)((base) + 0x14U)))

#define GPIO_MODE_OUTPUT_PP_2MHZ 0x2U

static uintptr_t gpio_port_base_from_id(int pin)
{
    switch (pin & 0xF0) {
    case 0x00: return GPIOA_BASE;
    case 0x10: return GPIOB_BASE;
    case 0x20: return GPIOC_BASE;
    case 0x30: return GPIOD_BASE;
    default: return 0U;
    }
}

static void gpio_port_clock_enable(int pin)
{
    switch (pin & 0xF0) {
    case 0x00:
        RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;
        break;
    case 0x10:
        RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;
        break;
    case 0x20:
        RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;
        break;
    case 0x30:
        RCC_APB2ENR |= RCC_APB2ENR_IOPDEN;
        break;
    default:
        break;
    }
}

static uint32_t gpio_pin_index(int pin)
{
    return (uint32_t)(pin & 0x0F);
}

static void gpio_write_raw(uintptr_t base, uint32_t gpio_pin, int level)
{
    if (base == 0U || gpio_pin > 15U) {
        return;
    }
    if (level != 0) {
        GPIO_BSRR(base) = (1UL << gpio_pin);
    } else {
        GPIO_BRR(base) = (1UL << gpio_pin);
    }
}

void bsp_gpio_config_output(int pin, int initial_level)
{
    uintptr_t base;
    uint32_t gpio_pin;
    volatile uint32_t *cfg_reg;
    uint32_t shift;
    uint32_t value;

    if (pin == BSP_GPIO_NONE) {
        return;
    }

    base = gpio_port_base_from_id(pin);
    gpio_pin = gpio_pin_index(pin);
    if (base == 0U || gpio_pin > 15U) {
        return;
    }

    gpio_port_clock_enable(pin);

    if (gpio_pin < 8U) {
        cfg_reg = (volatile uint32_t *)&GPIO_CRL(base);
        shift = gpio_pin * 4U;
    } else {
        cfg_reg = (volatile uint32_t *)&GPIO_CRH(base);
        shift = (gpio_pin - 8U) * 4U;
    }

    value = *cfg_reg;
    value &= ~(0xFUL << shift);
    value |= (GPIO_MODE_OUTPUT_PP_2MHZ << shift);
    *cfg_reg = value;

    gpio_write_raw(base, gpio_pin, initial_level);
}

void bsp_gpio_set(int pin, int level)
{
    uintptr_t base;
    uint32_t gpio_pin;

    if (pin == BSP_GPIO_NONE) {
        return;
    }
    base = gpio_port_base_from_id(pin);
    gpio_pin = gpio_pin_index(pin);
    gpio_write_raw(base, gpio_pin, level);
}

int bsp_gpio_get(int pin)
{
    uintptr_t base;
    uint32_t gpio_pin;

    if (pin == BSP_GPIO_NONE) {
        return 0;
    }
    base = gpio_port_base_from_id(pin);
    gpio_pin = gpio_pin_index(pin);
    if (base == 0U || gpio_pin > 15U) {
        return 0;
    }
    return ((GPIO_ODR(base) >> gpio_pin) & 1U) != 0U ? 1 : 0;
}

#else

void bsp_gpio_config_output(int pin, int initial_level)
{
    (void)pin;
    (void)initial_level;
}

void bsp_gpio_set(int pin, int level)
{
    (void)pin;
    (void)level;
}

int bsp_gpio_get(int pin)
{
    (void)pin;
    return 0;
}

#endif
