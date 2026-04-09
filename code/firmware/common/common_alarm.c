#include "common_alarm.h"
#include "common_status.h"
#include <string.h>

static uint32_t s_codes[COMMON_ALARM_MAX];
static size_t   s_count;

void common_alarm_init(void)
{
    common_alarm_clear();
}

void common_alarm_clear(void)
{
    memset(s_codes, 0, sizeof(s_codes));
    s_count = 0U;
    common_status_set_faults(NULL, 0U);
}

void common_alarm_raise(uint32_t code)
{
    if (s_count < COMMON_ALARM_MAX) {
        s_codes[s_count++] = code;
    }
    common_status_set_faults(s_codes, s_count);
}

void common_alarm_clear_code(uint32_t code)
{
    size_t w = 0U;
    for (size_t i = 0U; i < s_count; i++) {
        if (s_codes[i] != code) {
            s_codes[w++] = s_codes[i];
        }
    }
    s_count = w;
    common_status_set_faults(s_codes, s_count);
}

size_t common_alarm_copy(uint32_t *out, size_t max_codes)
{
    if (!out || max_codes == 0U) {
        return 0U;
    }
    size_t n = s_count < max_codes ? s_count : max_codes;
    memcpy(out, s_codes, n * sizeof(uint32_t));
    return n;
}
