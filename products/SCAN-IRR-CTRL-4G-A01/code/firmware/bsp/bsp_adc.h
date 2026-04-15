#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

/* Power-on init, called once from app_main_init(). */
void bsp_adc_init(void);

/* Raw ADC sample from the ACC sense node on PA0 after BAT_TEST enables Q4. */
uint16_t bsp_adc_read_battery_raw(void);

/* Samples battery voltage and updates common_status. */
void bsp_adc_sample_battery_to_status(void);

#endif /* BSP_ADC_H */
