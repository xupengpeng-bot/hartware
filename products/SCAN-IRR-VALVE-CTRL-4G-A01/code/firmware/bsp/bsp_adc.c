#include "bsp_adc.h"
#include "board_hw_config.h"
#include "bsp_system.h"
#include "common_status.h"

#include <stdint.h>

/*
 * Battery detect block from the schematic:
 * - BAT_TEST enable: 机井3.0 = PA12；旧板 = PB12（见 board_hw_config.h）。
 * - PA0 samples the ACC node.
 * - R22 = 10k from battery rail to the sense path.
 * - R25 = 3.3k from ACC to GND.
 */
#define BAT_SENSE_R22_OHM 10000U
#define BAT_SENSE_R25_OHM 3300U

#define BAT_VREF_MV       3300U
#define BAT_ADC_MAX       4095U

/*
 * Battery SOC calibration for the deployed 12V pack:
 * - 12.0V = 100%
 * - 10.5V = 0%
 * We keep the voltage telemetry untouched and only remap the percentage.
 */
#define BAT_EMPTY_MV      10500U
#define BAT_FULL_MV       12000U

#if defined(BOARD_STM32F103)

#define RCC_BASE    0x40021000U
#define GPIOA_BASE  0x40010800U
#define GPIOB_BASE  0x40010C00U
#define ADC1_BASE   0x40012400U

#define RCC_APB2ENR (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define RCC_CFGR    (*((volatile uint32_t *)(RCC_BASE + 0x04U)))

#define GPIOA_CRL   (*((volatile uint32_t *)(GPIOA_BASE + 0x00U)))
#define GPIOA_CRH   (*((volatile uint32_t *)(GPIOA_BASE + 0x04U)))
#define GPIOA_BSRR  (*((volatile uint32_t *)(GPIOA_BASE + 0x10U)))
#define GPIOB_CRH   (*((volatile uint32_t *)(GPIOB_BASE + 0x04U)))
#define GPIOB_BSRR  (*((volatile uint32_t *)(GPIOB_BASE + 0x10U)))

#define ADC1_SR     (*((volatile uint32_t *)(ADC1_BASE + 0x00U)))
#define ADC1_CR1    (*((volatile uint32_t *)(ADC1_BASE + 0x04U)))
#define ADC1_CR2    (*((volatile uint32_t *)(ADC1_BASE + 0x08U)))
#define ADC1_SMPR2  (*((volatile uint32_t *)(ADC1_BASE + 0x10U)))
#define ADC1_SQR1   (*((volatile uint32_t *)(ADC1_BASE + 0x2CU)))
#define ADC1_SQR3   (*((volatile uint32_t *)(ADC1_BASE + 0x34U)))
#define ADC1_DR     (*((volatile uint32_t *)(ADC1_BASE + 0x4CU)))

#define RCC_APB2ENR_IOPAEN    (1U << 2)
#define RCC_APB2ENR_IOPBEN    (1U << 3)
#define RCC_APB2ENR_ADC1EN    (1U << 9)
#define RCC_CFGR_ADCPRE_DIV6  (2U << 14)

#define ADC_CR2_ADON          (1U << 0)
#define ADC_CR2_CAL           (1U << 2)
#define ADC_CR2_RSTCAL        (1U << 3)
#define ADC_CR2_SWSTART       (1U << 22)
#define ADC_SR_EOC            (1U << 1)

static uint8_t s_adc_ready;

static void battery_measure_enable(int enable)
{
#if BOARD_HW_BAT_TEST_ON_GPIOA
    if (enable != 0) {
        GPIOA_BSRR = (1U << BOARD_HW_PIN_BAT_TEST_ENABLE);
    } else {
        GPIOA_BSRR = (1U << (BOARD_HW_PIN_BAT_TEST_ENABLE + 16U));
    }
#else
    if (enable != 0) {
        GPIOB_BSRR = (1U << BOARD_HW_PIN_BAT_TEST_ENABLE);
    } else {
        GPIOB_BSRR = (1U << (BOARD_HW_PIN_BAT_TEST_ENABLE + 16U));
    }
#endif
}

static uint16_t adc_single_read(void)
{
    ADC1_CR2 |= ADC_CR2_ADON;
    ADC1_CR2 |= ADC_CR2_SWSTART;
    for (uint32_t wait = 0U; wait < 100000U; wait++) {
        if ((ADC1_SR & ADC_SR_EOC) != 0U) {
            return (uint16_t)(ADC1_DR & 0x0FFFU);
        }
    }
    return 0U;
}

static void stm32_adc_hw_init(void)
{
    if (s_adc_ready != 0U) {
        return;
    }

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_ADC1EN;
    RCC_CFGR |= RCC_CFGR_ADCPRE_DIV6;

    /* PA0 analog input: MODE=00, CNF=00. */
    {
        uint32_t crl = GPIOA_CRL;
        crl &= ~(0xFU << 0U);
        GPIOA_CRL = crl;
    }

    /* PA12 (机井3.0) or PB12 (旧板): push-pull output for BAT_TEST enable. */
#if BOARD_HW_BAT_TEST_ON_GPIOA
    {
        uint32_t crh = GPIOA_CRH;
        crh &= ~(0xFU << 16U);
        crh |= (0x3U << 16U);
        GPIOA_CRH = crh;
    }
#else
    {
        uint32_t crh = GPIOB_CRH;
        crh &= ~(0xFU << 16U);
        crh |= (0x3U << 16U);
        GPIOB_CRH = crh;
    }
#endif
    battery_measure_enable(0);

    ADC1_CR1 = 0U;
    ADC1_CR2 = ADC_CR2_ADON;
    for (volatile uint32_t d = 0U; d < 1000U; d++) {
        __asm volatile("" ::: "memory");
    }

    ADC1_CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1_CR2 & ADC_CR2_RSTCAL) != 0U) {
    }
    ADC1_CR2 |= ADC_CR2_CAL;
    while ((ADC1_CR2 & ADC_CR2_CAL) != 0U) {
    }

    /* Channel 0 (PA0) sample time = 239.5 cycles. */
    ADC1_SMPR2 &= ~(7U << 0U);
    ADC1_SMPR2 |= (7U << 0U);
    ADC1_SQR1 = 0U;
    ADC1_SQR3 = (BOARD_HW_ADC1_CHANNEL_BAT_TEST & 0x1FU);

    s_adc_ready = 1U;
}

void bsp_adc_init(void)
{
    stm32_adc_hw_init();
}

uint16_t bsp_adc_read_battery_raw(void)
{
    uint16_t raw;

    stm32_adc_hw_init();

    battery_measure_enable(1);
    bsp_system_delay_ms(3U);
    (void)adc_single_read();
    bsp_system_delay_ms(1U);
    raw = adc_single_read();
    battery_measure_enable(0);

    return raw;
}

#else

void bsp_adc_init(void)
{
}

uint16_t bsp_adc_read_battery_raw(void)
{
    /* Host simulation: about 12.0V battery through the ACC divider. */
    return 3700U;
}

#endif /* BOARD_STM32F103 */

void bsp_adc_sample_battery_to_status(void)
{
    uint32_t raw = (uint32_t)bsp_adc_read_battery_raw();
    if (raw > BAT_ADC_MAX) {
        raw = BAT_ADC_MAX;
    }

    {
        uint32_t v_acc_mv = raw * BAT_VREF_MV / BAT_ADC_MAX;
        uint32_t vbat_mv = v_acc_mv * (BAT_SENSE_R22_OHM + BAT_SENSE_R25_OHM) / BAT_SENSE_R25_OHM;
        float vbat_v = (float)vbat_mv / 1000.0f;
        uint8_t soc = 0U;
        float vsolar = 0.0f;

        if (vbat_mv >= BAT_FULL_MV) {
            soc = 100U;
        } else if (vbat_mv > BAT_EMPTY_MV) {
            soc = (uint8_t)((vbat_mv - BAT_EMPTY_MV) * 100U / (BAT_FULL_MV - BAT_EMPTY_MV));
        }

        common_status_set_battery(soc, vbat_v, vsolar);
    }
}
