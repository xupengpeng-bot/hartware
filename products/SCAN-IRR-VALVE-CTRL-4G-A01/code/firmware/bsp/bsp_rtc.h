#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>
#include <stddef.h>

int bsp_rtc_get_unix(uint32_t *out);
int bsp_rtc_set_unix(uint32_t unix_sec);
int bsp_rtc_is_synced(void);
void bsp_rtc_reset(void);
void bsp_rtc_tick(uint32_t monotonic_ms);
int bsp_rtc_configured_timezone_qh(void);
int bsp_rtc_format_iso8601(char *out, size_t cap, uint32_t unix_sec, int tzq);
int bsp_rtc_now_iso8601(char *out, size_t cap, int *out_tzq);
int bsp_rtc_format_iso8601_utc(char *out, size_t cap, uint32_t unix_sec);
int bsp_rtc_now_iso8601_utc(char *out, size_t cap);

#endif /* BSP_RTC_H */
