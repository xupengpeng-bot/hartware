#include "bsp_rtc.h"

int bsp_rtc_get_unix(uint32_t *out)
{
    if (!out) {
        return -1;
    }
    *out = 0U;
    return 0;
}
