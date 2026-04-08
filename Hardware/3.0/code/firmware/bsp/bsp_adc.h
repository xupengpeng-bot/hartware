#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

/** 上电后调用一次（app_main_init 内） */
void bsp_adc_init(void);

/** 原始 ADC 值 0..4095，通道为 BOARD_HW_ADC1_CHANNEL_BAT_TEST（PA1） */
uint16_t bsp_adc_read_battery_raw(void);

/** 采样并写入 common_status（电量、电压） */
void bsp_adc_sample_battery_to_status(void);

#endif /* BSP_ADC_H */
