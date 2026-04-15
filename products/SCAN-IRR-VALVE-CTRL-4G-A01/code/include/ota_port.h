/**
 * Platform port — HTTP, flash, crypto, reboot. Implement in board/BSP layer.
 */
#ifndef OTA_PORT_H
#define OTA_PORT_H

#include "ota_types.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool (*tcp_session_stable)(void *user);
    int  (*get_battery_soc)(uint8_t *out_pct, void *user);
    int  (*get_signal_csq)(int16_t *out_csq, void *user);
    int (*http_download_chunk)(const ota_prepare_payload_t *manifest, uint32_t offset, uint8_t *buf,
                               size_t buf_len, size_t *out_read, void *user);
    int (*sha256_init)(void **ctx, void *user);
    int (*sha256_update)(void *ctx, const uint8_t *data, size_t len, void *user);
    int (*sha256_final)(void *ctx, uint8_t out32[32], void *user);
    void (*sha256_free)(void *ctx, void *user);
    int (*flash_erase_upgrade_region)(void *user);
    int (*flash_write_upgrade_region)(uint32_t offset, const uint8_t *data, size_t len,
                                      void *user);
    int  (*reboot_to_new_image)(void *user);
    int  (*reboot_mcu)(void *user);
    int  (*storage_free_bytes)(uint32_t *out_free, void *user);
    void *user;
} ota_port_t;

#endif /* OTA_PORT_H */
