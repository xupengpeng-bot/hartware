#include "boot_diag.h"

#include "app_context.h"
#include "bsp_rtc.h"

#include <stdio.h>

#define BOOT_DIAG_RCC_CSR_PINRSTF  (1U << 26)
#define BOOT_DIAG_RCC_CSR_PORRSTF  (1U << 27)
#define BOOT_DIAG_RCC_CSR_SFTRSTF  (1U << 28)
#define BOOT_DIAG_RCC_CSR_IWDGRSTF (1U << 29)
#define BOOT_DIAG_RCC_CSR_WWDGRSTF (1U << 30)
#define BOOT_DIAG_RCC_CSR_LPWRRSTF (1U << 31)

static uint32_t s_reset_csr_raw;
static uint8_t s_reset_reason = BOOT_DIAG_RESET_UNKNOWN;

static uint8_t boot_diag_reason_from_csr(uint32_t csr_raw)
{
    if ((csr_raw & BOOT_DIAG_RCC_CSR_IWDGRSTF) != 0U) {
        return BOOT_DIAG_RESET_IWDG;
    }
    if ((csr_raw & BOOT_DIAG_RCC_CSR_WWDGRSTF) != 0U) {
        return BOOT_DIAG_RESET_WWDG;
    }
    if ((csr_raw & BOOT_DIAG_RCC_CSR_SFTRSTF) != 0U) {
        return BOOT_DIAG_RESET_SOFT;
    }
    if ((csr_raw & BOOT_DIAG_RCC_CSR_PORRSTF) != 0U) {
        return BOOT_DIAG_RESET_POR;
    }
    if ((csr_raw & BOOT_DIAG_RCC_CSR_PINRSTF) != 0U) {
        return BOOT_DIAG_RESET_PIN;
    }
    if ((csr_raw & BOOT_DIAG_RCC_CSR_LPWRRSTF) != 0U) {
        return BOOT_DIAG_RESET_LPWR;
    }
    return BOOT_DIAG_RESET_UNKNOWN;
}

void boot_diag_capture_reset_csr(uint32_t csr_raw)
{
    s_reset_csr_raw = csr_raw;
    s_reset_reason = boot_diag_reason_from_csr(csr_raw);
}

uint8_t boot_diag_reset_reason_code(void)
{
    return s_reset_reason;
}

uint32_t boot_diag_reset_csr_raw(void)
{
    return s_reset_csr_raw;
}

const char *boot_diag_reset_reason_name(uint8_t reason)
{
    switch (reason) {
    case BOOT_DIAG_RESET_PIN: return "pin";
    case BOOT_DIAG_RESET_POR: return "power_on";
    case BOOT_DIAG_RESET_SOFT: return "software_reset";
    case BOOT_DIAG_RESET_IWDG:
    case BOOT_DIAG_RESET_WWDG:
        return "watchdog";
    case BOOT_DIAG_RESET_LPWR: return "brownout";
    case BOOT_DIAG_RESET_UNKNOWN:
    default:
        return "unknown";
    }
}

uint32_t boot_diag_uptime_sec(void)
{
    return app_context()->monotonic_ms / 1000U;
}

int boot_diag_format_boot_session_id(char *out, size_t cap)
{
    uint32_t now_unix = 0U;
    uint32_t uptime_sec = boot_diag_uptime_sec();

    if (out == NULL || cap < 24U) {
        return -1;
    }

    if (bsp_rtc_get_unix(&now_unix) == 0 && now_unix >= uptime_sec) {
        return snprintf(out, cap, "boot-%lu-r%u",
                        (unsigned long)(now_unix - uptime_sec),
                        (unsigned)boot_diag_reset_reason_code());
    }

    return snprintf(out, cap, "unsynced-r%u-%08lX",
                    (unsigned)boot_diag_reset_reason_code(),
                    (unsigned long)boot_diag_reset_csr_raw());
}
