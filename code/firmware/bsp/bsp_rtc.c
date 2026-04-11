#include "bsp_rtc.h"
#include "runtime_state.h"
#include "storage_config.h"
#include <stdio.h>
#include <time.h>

static uint32_t s_rtc_unix_sec;
static int      s_rtc_synced;
static uint32_t s_last_monotonic_ms;
static uint16_t s_subsec_ms;

int bsp_rtc_get_unix(uint32_t *out)
{
    if (!out) {
        return -1;
    }
    *out = s_rtc_unix_sec;
    return s_rtc_synced ? 0 : -2;
}

int bsp_rtc_set_unix(uint32_t unix_sec)
{
    if (unix_sec == 0U) {
        s_rtc_unix_sec = 0U;
        s_rtc_synced = 0;
        s_subsec_ms = 0U;
        runtime_state_set_time_synced(false);
        return -1;
    }
    s_rtc_unix_sec = unix_sec;
    s_rtc_synced = 1;
    s_subsec_ms = 0U;
    runtime_state_set_time_synced(true);
    return 0;
}

int bsp_rtc_is_synced(void)
{
    return s_rtc_synced;
}

void bsp_rtc_reset(void)
{
    s_rtc_unix_sec = 0U;
    s_rtc_synced = 0;
    s_last_monotonic_ms = 0U;
    s_subsec_ms = 0U;
    runtime_state_set_time_synced(false);
}

void bsp_rtc_tick(uint32_t monotonic_ms)
{
    uint32_t delta_ms;
    uint32_t acc_ms;

    if (s_last_monotonic_ms == 0U) {
        s_last_monotonic_ms = monotonic_ms;
        return;
    }
    delta_ms = monotonic_ms - s_last_monotonic_ms;
    s_last_monotonic_ms = monotonic_ms;
    if (s_rtc_synced == 0 || delta_ms == 0U) {
        return;
    }

    acc_ms = (uint32_t)s_subsec_ms + delta_ms;
    while (acc_ms >= 1000U) {
        acc_ms -= 1000U;
        s_rtc_unix_sec++;
    }
    s_subsec_ms = (uint16_t)acc_ms;
}

int bsp_rtc_configured_timezone_qh(void)
{
    const device_config_t *cfg = storage_config_active();

    if (cfg != NULL && cfg->time_zone_quarter_hours >= -48 && cfg->time_zone_quarter_hours <= 56) {
        return cfg->time_zone_quarter_hours;
    }
    return MODEL_DEFAULT_TIME_ZONE_QUARTER_HOURS;
}

int bsp_rtc_format_iso8601(char *out, size_t cap, uint32_t unix_sec, int tzq)
{
    int64_t offset_minutes;
    int64_t local_unix;
    time_t raw;
    struct tm *tm_local;
    int abs_minutes;
    char sign;
    int n;

    if (out == NULL || cap < 26U) {
        return -1;
    }
    if (tzq < -48 || tzq > 56) {
        return -2;
    }

    offset_minutes = (int64_t)tzq * 15LL;
    local_unix = (int64_t)unix_sec + offset_minutes * 60LL;
    if (local_unix < 0LL) {
        return -3;
    }

    raw = (time_t)local_unix;
    tm_local = gmtime(&raw);
    if (tm_local == NULL) {
        return -4;
    }

    sign = (tzq < 0) ? '-' : '+';
    abs_minutes = (tzq < 0) ? -tzq * 15 : tzq * 15;
    n = snprintf(out, cap,
                 "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                 tm_local->tm_year + 1900,
                 tm_local->tm_mon + 1,
                 tm_local->tm_mday,
                 tm_local->tm_hour,
                 tm_local->tm_min,
                 tm_local->tm_sec,
                 sign,
                 abs_minutes / 60,
                 abs_minutes % 60);
    if (n < 0 || (size_t)n >= cap) {
        return -5;
    }
    return 0;
}

int bsp_rtc_now_iso8601(char *out, size_t cap, int *out_tzq)
{
    uint32_t unix_sec = 0U;
    int tzq = bsp_rtc_configured_timezone_qh();

    if (bsp_rtc_get_unix(&unix_sec) != 0 || unix_sec == 0U) {
        return -1;
    }
    if (bsp_rtc_format_iso8601(out, cap, unix_sec, tzq) != 0) {
        return -2;
    }
    if (out_tzq != NULL) {
        *out_tzq = tzq;
    }
    return 0;
}

int bsp_rtc_format_iso8601_utc(char *out, size_t cap, uint32_t unix_sec)
{
    time_t raw;
    struct tm *tm_utc;
    int n;

    if (out == NULL || cap < 21U) {
        return -1;
    }

    raw = (time_t)unix_sec;
    tm_utc = gmtime(&raw);
    if (tm_utc == NULL) {
        return -2;
    }

    n = snprintf(out, cap,
                 "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 tm_utc->tm_year + 1900,
                 tm_utc->tm_mon + 1,
                 tm_utc->tm_mday,
                 tm_utc->tm_hour,
                 tm_utc->tm_min,
                 tm_utc->tm_sec);
    if (n < 0 || (size_t)n >= cap) {
        return -3;
    }
    return 0;
}

int bsp_rtc_now_iso8601_utc(char *out, size_t cap)
{
    uint32_t unix_sec = 0U;
    if (bsp_rtc_get_unix(&unix_sec) != 0 || unix_sec == 0U) {
        return -1;
    }
    return bsp_rtc_format_iso8601_utc(out, cap, unix_sec);
}
