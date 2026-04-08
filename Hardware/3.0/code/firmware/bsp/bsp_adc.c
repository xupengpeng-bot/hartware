#include "bsp_adc.h"
#include "board_hw_config.h"
#include "common_status.h"

#include <stdint.h>

/* 原理图 BAT_TESTR23：R23=4.7k 上拉至电池、R24=47k 下拉到地时
 * Vadc = Vbat * R24/(R23+R24) => Vbat_mV = Vadc_mV * (R23+R24) / R24 */
#define BAT_DIV_R23_OHM 4700U
#define BAT_DIV_R24_OHM 47000U

#define BAT_VREF_MV     3300U
#define BAT_ADC_MAX     4095U

#define BAT_EMPTY_MV    10500U
#define BAT_FULL_MV     16800U

#if defined(BOARD_STM32F103)

#define RCC_BASE   0x40021000U
#define GPIOA_BASE 0x40010800U
#define ADC1_BASE  0x40012400U

#define RCC_APB2ENR  (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define RCC_CFGR     (*((volatile uint32_t *)(RCC_BASE + 0x04U)))

#define GPIOA_CRL (*((volatile uint32_t *)(GPIOA_BASE + 0x00U)))

#define ADC1_SR  (*((volatile uint32_t *)(ADC1_BASE + 0x00U)))
#define ADC1_CR1 (*((volatile uint32_t *)(ADC1_BASE + 0x04U)))
#define ADC1_CR2 (*((volatile uint32_t *)(ADC1_BASE + 0x08U)))
#define ADC1_SMPR2 (*((volatile uint32_t *)(ADC1_BASE + 0x10U)))
#define ADC1_SQR1 (*((volatile uint32_t *)(ADC1_BASE + 0x2CU)))
#define ADC1_SQR3 (*((volatile uint32_t *)(ADC1_BASE + 0x34U)))
#define ADC1_DR (*((volatile uint32_t *)(ADC1_BASE + 0x4CU)))

#define RCC_APB2ENR_IOPAEN (1U << 2)
#define RCC_APB2ENR_ADC1EN (1U << 9)
#define RCC_CFGR_ADCPRE_DIV6 (2U << 14)

#define ADC_CR2_ADON   (1U << 0)
#define ADC_CR2_CAL    (1U << 2)
#define ADC_CR2_RSTCAL (1U << 3)
#define ADC_CR2_SWSTART (1U << 22)
#define ADC_SR_EOC     (1U << 1)

static uint8_t s_adc_ready;

static void stm32_adc_hw_init(void)
{
    if (s_adc_ready != 0U) {
        return;
    }

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    RCC_CFGR |= RCC_CFGR_ADCPRE_DIV6;

    /* PA1 模拟输入：CNF=00 MODE=00 */
    {
        uint32_t crl = GPIOA_CRL;
        crl &= ~(0xFU << 4U);
        crl |= (0x0U << 4U);
        GPIOA_CRL = crl;
    }

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

    /* 通道 1（PA1）采样时间 239.5 */
    ADC1_SMPR2 |= (7U << 3U);
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
    stm32_adc_hw_init();

    ADC1_CR2 |= ADC_CR2_ADON;
    ADC1_CR2 |= ADC_CR2_SWSTART;
    for (uint32_t wait = 0U; wait < 100000U; wait++) {
        if ((ADC1_SR & ADC_SR_EOC) != 0U) {
            return (uint16_t)(ADC1_DR & 0xFFFU);
        }
    }
    return 0U;
}

#else

void bsp_adc_init(void)
{
}

uint16_t bsp_adc_read_battery_raw(void)
{
    /* 主机仿真：约 12.6V 电池（与分压公式一致） */
    return 1400U;
}

#endif /* BOARD_STM32F103 */

void bsp_adc_sample_battery_to_status(void)
{
    uint32_t raw = (uint32_t)bsp_adc_read_battery_raw();
    if (raw > BAT_ADC_MAX) {
        raw = BAT_ADC_MAX;
    }

    uint32_t v_adc_mv = raw * BAT_VREF_MV / BAT_ADC_MAX;
    uint32_t vbat_mv  = v_adc_mv * (BAT_DIV_R23_OHM + BAT_DIV_R24_OHM) / BAT_DIV_R24_OHM;

    float vbat_v = (float)vbat_mv / 1000.0f;

    uint8_t soc = 0U;
    if (vbat_mv >= BAT_FULL_MV) {
        soc = 100U;
    } else if (vbat_mv <= BAT_EMPTY_MV) {
        soc = 0U;
    } else {
        soc = (uint8_t)((vbat_mv - BAT_EMPTY_MV) * 100U / (BAT_FULL_MV - BAT_EMPTY_MV));
    }

    float vsolar = 0.0f;
    common_status_set_battery(soc, vbat_v, vsolar);
}
