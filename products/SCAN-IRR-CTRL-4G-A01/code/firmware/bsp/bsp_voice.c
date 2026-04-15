#include "bsp_voice.h"

#include "voice_chip_prompts.h"

#include "bsp_system.h"
#include "bsp_uart.h"

#if defined(BOARD_STM32F103)
#include "board_clock_stm32f103.h"
#include "board_hw_config.h"

#include <stdint.h>
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char   *prompt_code;
    unsigned char chip_code;
} voice_prompt_alias_t;

typedef struct {
    unsigned char chip_code;
    const char   *phrase;
} voice_chip_phrase_t;

static const voice_chip_phrase_t s_chip_phrases[] = {
#define VOICE_CHIP_PROMPT_ENTRY(code, text) { (unsigned char)(code), text },
    VOICE_CHIP_PROMPT_TABLE(VOICE_CHIP_PROMPT_ENTRY)
#undef VOICE_CHIP_PROMPT_ENTRY
};

static const voice_prompt_alias_t s_prompt_aliases[] = {
    { "welcome", 18U },
    { "invalid_card", 25U },
    { "device_booting", 18U },
    { "device_not_ready", 36U },
    { "device_config_missing", 36U },
    { "network_unavailable", 36U },
    { "auth_checking", 28U },
    { "auth_granted", 34U },
    { "auth_denied", 36U },
    { "meter_query_failed", 29U },
    { "starting_pump", 28U },
    { "starting_valve", 28U },
    { "start_success", 34U },
    { "start_failed", 36U },
    { "already_running", 27U },
    { "port_busy", 27U },
    { "stopping", 35U },
    { "stopped", 35U },
    { "protection_triggered", 32U },
    { "device_fault", 32U },
    { "power_interrupted", 32U },
    { "power_restored_self_check", 18U },
    { "recovery_locked_confirm_required", 36U },
    { "irrigation_started", 34U },
    { "irrigation_finished", 35U },
    { "irrigation_paused", 19U },
    { "irrigation_resumed", 33U },
    { "starting_wait", 28U },
    { "unavailable", 36U }
};

#if defined(BOARD_STM32F103) && BOARD_HW_HAS_VOICE_CHIP

#define RCC_BASE         0x40021000U
#define GPIOA_BASE       0x40010800U
#define GPIOC_BASE       0x40011000U
#define RCC_APB2ENR      (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define RCC_APB2ENR_IOPAEN (1U << 2)
#define RCC_APB2ENR_IOPCEN (1U << 4)

#define GPIO_CRL(base)   (*((volatile uint32_t *)((base) + 0x00U)))
#define GPIO_CRH(base)   (*((volatile uint32_t *)((base) + 0x04U)))
#define GPIO_IDR(base)   (*((volatile uint32_t *)((base) + 0x08U)))
#define GPIO_ODR(base)   (*((volatile uint32_t *)((base) + 0x0CU)))
#define GPIO_BSRR(base)  (*((volatile uint32_t *)((base) + 0x10U)))
#define GPIO_BRR(base)   (*((volatile uint32_t *)((base) + 0x14U)))

#define GPIO_MODE_INPUT_FLOATING 0x4U
#define GPIO_MODE_INPUT_PULL     0x8U
#define GPIO_MODE_OUTPUT_PP_2MHZ 0x2U

#define VOICE_ONE_LINE_BITS        8U
#define VOICE_RESET_LOW_MS         5U
#define VOICE_RESET_HIGH_MS        20U
#define VOICE_START_LOW_MS         5U
#define VOICE_BIT_SHORT_US         200U
#define VOICE_BIT_LONG_US          600U
#define VOICE_INTER_PROMPT_GUARD_MS 24U

static uint8_t s_voice_initialized;

static uintptr_t voice_port_base_from_pin(char port_tag)
{
    switch (port_tag) {
    case 'A': return GPIOA_BASE;
    case 'C': return GPIOC_BASE;
    default: return 0U;
    }
}

static void voice_gpio_config(uintptr_t base, uint32_t pin, uint32_t mode_bits)
{
    volatile uint32_t *cfg_reg;
    uint32_t           shift;
    uint32_t           reg;

    if (base == 0U || pin > 15U) {
        return;
    }

    if (pin < 8U) {
        cfg_reg = (volatile uint32_t *)&GPIO_CRL(base);
        shift = pin * 4U;
    } else {
        cfg_reg = (volatile uint32_t *)&GPIO_CRH(base);
        shift = (pin - 8U) * 4U;
    }

    reg = *cfg_reg;
    reg &= ~(0xFU << shift);
    reg |= ((mode_bits & 0xFU) << shift);
    *cfg_reg = reg;
}

static void voice_gpio_write(uintptr_t base, uint32_t pin, int level)
{
    if (base == 0U || pin > 15U) {
        return;
    }

    if (level != 0) {
        GPIO_BSRR(base) = (1UL << pin);
    } else {
        GPIO_BRR(base) = (1UL << pin);
    }
}

static int voice_gpio_read(uintptr_t base, uint32_t pin)
{
    if (base == 0U || pin > 15U) {
        return 0;
    }
    return ((GPIO_IDR(base) >> pin) & 1U) != 0U ? 1 : 0;
}

static void voice_delay_us(uint32_t us)
{
    volatile uint32_t loops;

    loops = (SystemCoreClock / 5000000U) * us;
    if (loops == 0U) {
        loops = us;
    }

    while (loops > 0U) {
        loops--;
    }
}

static void voice_line_data(int level)
{
    voice_gpio_write(voice_port_base_from_pin('C'), BOARD_HW_PIN_VOICE_DATA_PORT_C, level);
}

static void voice_line_reset(int level)
{
    voice_gpio_write(voice_port_base_from_pin('C'), BOARD_HW_PIN_VOICE_RESET_PORT_C, level);
}

static int voice_line_busy_level(void)
{
    return voice_gpio_read(voice_port_base_from_pin('A'), BOARD_HW_PIN_VOICE_BUSY_PORT_A);
}

static void voice_prepare_bus(void)
{
    voice_line_data(1);
    voice_line_reset(1);
}

static void voice_send_one_line(unsigned char code)
{
    uint32_t bit_idx;

    voice_prepare_bus();

    voice_line_reset(0);
    bsp_system_delay_ms(VOICE_RESET_LOW_MS);
    voice_line_reset(1);
    bsp_system_delay_ms(VOICE_RESET_HIGH_MS);

    voice_line_data(0);
    bsp_system_delay_ms(VOICE_START_LOW_MS);

    for (bit_idx = 0U; bit_idx < VOICE_ONE_LINE_BITS; ++bit_idx) {
        voice_line_data(1);
        if ((code & 0x01U) != 0U) {
            voice_delay_us(VOICE_BIT_LONG_US);
            voice_line_data(0);
            voice_delay_us(VOICE_BIT_SHORT_US);
        } else {
            voice_delay_us(VOICE_BIT_SHORT_US);
            voice_line_data(0);
            voice_delay_us(VOICE_BIT_LONG_US);
        }
        code >>= 1;
    }

    voice_line_data(1);
    bsp_system_delay_ms(VOICE_INTER_PROMPT_GUARD_MS);
}

static void voice_hw_init(void)
{
    uintptr_t port_a = voice_port_base_from_pin('A');
    uintptr_t port_c = voice_port_base_from_pin('C');

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    voice_gpio_config(port_c, BOARD_HW_PIN_VOICE_DATA_PORT_C, GPIO_MODE_OUTPUT_PP_2MHZ);
    voice_gpio_config(port_c, BOARD_HW_PIN_VOICE_RESET_PORT_C, GPIO_MODE_OUTPUT_PP_2MHZ);
    voice_gpio_config(port_a, BOARD_HW_PIN_VOICE_BUSY_PORT_A, GPIO_MODE_INPUT_PULL);
    GPIO_ODR(port_a) |= (1UL << BOARD_HW_PIN_VOICE_BUSY_PORT_A);

    voice_prepare_bus();
    s_voice_initialized = 1U;
}

#endif

static const char *bsp_voice_phrase_by_id(unsigned char chip_code)
{
    size_t idx;

    for (idx = 0U; idx < (sizeof(s_chip_phrases) / sizeof(s_chip_phrases[0])); ++idx) {
        if (s_chip_phrases[idx].chip_code == chip_code) {
            return s_chip_phrases[idx].phrase;
        }
    }
    return "";
}

static int bsp_voice_lookup_prompt(const char *prompt_code, unsigned char *chip_code_out, const char **phrase_out)
{
    size_t idx;

    if (prompt_code == NULL || chip_code_out == NULL || phrase_out == NULL) {
        return -1;
    }

    for (idx = 0U; idx < (sizeof(s_prompt_aliases) / sizeof(s_prompt_aliases[0])); ++idx) {
        if (strcmp(s_prompt_aliases[idx].prompt_code, prompt_code) == 0) {
            *chip_code_out = s_prompt_aliases[idx].chip_code;
            *phrase_out = bsp_voice_phrase_by_id(*chip_code_out);
            return 0;
        }
    }

    return -1;
}

void bsp_voice_init(void)
{
#if defined(BOARD_STM32F103) && BOARD_HW_HAS_VOICE_CHIP
    if (s_voice_initialized == 0U) {
        voice_hw_init();
    }
#endif
}

bool bsp_voice_supported(void)
{
#if defined(BOARD_STM32F103) && BOARD_HW_HAS_VOICE_CHIP
    return true;
#else
    return false;
#endif
}

bool bsp_voice_is_busy(void)
{
#if defined(BOARD_STM32F103) && BOARD_HW_HAS_VOICE_CHIP
    if (s_voice_initialized == 0U) {
        return false;
    }
    return voice_line_busy_level() == (int)BOARD_HW_VOICE_BUSY_ACTIVE_LEVEL;
#else
    return false;
#endif
}

int bsp_voice_play_prompt(const char *prompt_code)
{
    unsigned char chip_code = 0U;
    const char   *phrase = "";
    char          line[160];
    int           busy_before = 0;

    if (prompt_code == NULL || prompt_code[0] == '\0') {
        return -1;
    }

    if (bsp_voice_lookup_prompt(prompt_code, &chip_code, &phrase) != 0) {
        (void)snprintf(line, sizeof(line), "[VOICE] unmapped prompt=%s\r\n", prompt_code);
        bsp_debug_log(line);
        return -2;
    }

    if (!bsp_voice_supported()) {
        (void)snprintf(line, sizeof(line),
                       "[VOICE] unsupported prompt=%s chip_code=%u phrase=%s\r\n",
                       prompt_code,
                       (unsigned)chip_code,
                       phrase);
        bsp_debug_log(line);
        return -3;
    }

    bsp_voice_init();

    busy_before = bsp_voice_is_busy() ? 1 : 0;
    if (busy_before != 0) {
        (void)snprintf(line, sizeof(line),
                       "[VOICE] busy_line_active_before_tx prompt=%s chip_code=%u phrase=%s\r\n",
                       prompt_code,
                       (unsigned)chip_code,
                       phrase);
        bsp_debug_log(line);
    }

#if defined(BOARD_STM32F103) && BOARD_HW_HAS_VOICE_CHIP
    voice_send_one_line(chip_code);
#endif

    (void)snprintf(line, sizeof(line),
                   "[VOICE] prompt=%s chip_code=%u phrase=%s tx=one_line busy_before=%d busy_after=%d\r\n",
                   prompt_code,
                   (unsigned)chip_code,
                   phrase,
                   busy_before,
                   bsp_voice_is_busy() ? 1 : 0);
    bsp_debug_log(line);

    return (int)chip_code;
}
