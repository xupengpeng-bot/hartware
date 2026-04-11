#ifndef BOOT_DIAG_H
#define BOOT_DIAG_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    BOOT_DIAG_RESET_UNKNOWN = 0,
    BOOT_DIAG_RESET_PIN = 1,
    BOOT_DIAG_RESET_POR = 2,
    BOOT_DIAG_RESET_SOFT = 3,
    BOOT_DIAG_RESET_IWDG = 4,
    BOOT_DIAG_RESET_WWDG = 5,
    BOOT_DIAG_RESET_LPWR = 6
} boot_diag_reset_reason_t;

void boot_diag_capture_reset_csr(uint32_t csr_raw);
uint8_t boot_diag_reset_reason_code(void);
uint32_t boot_diag_reset_csr_raw(void);
const char *boot_diag_reset_reason_name(uint8_t reason);
uint32_t boot_diag_uptime_sec(void);
int boot_diag_format_boot_session_id(char *out, size_t cap);

#endif /* BOOT_DIAG_H */
