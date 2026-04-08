#include "workflow_session.h"
#include "bsp_rtc.h"
#include <stdio.h>
#include <string.h>

void workflow_session_generate_id(char *out, size_t out_sz)
{
    if (!out || out_sz < 8U) {
        return;
    }
    uint32_t sec = 0U;
    (void)bsp_rtc_get_unix(&sec);
    (void)snprintf(out, out_sz, "S-%08lx", (unsigned long)sec);
}
