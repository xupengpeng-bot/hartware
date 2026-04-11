#include "ota_port_board.h"
#include "bsp_adc.h"
#include "bsp_flash.h"
#include "bsp_uart.h"
#include "boot_control.h"
#include "common_status.h"
#include "flash_layout.h"
#include "storage_upgrade.h"

#include <stddef.h>

#define SCB_AIRCR_ADDR       0xE000ED0Cu
#define SCB_AIRCR_SYSRESET   0x05FA0004u

static bool read_tcp_ok(void *user)
{
    (void)user;
    return common_status_get()->tcp_connected;
}

static int read_battery_soc(uint8_t *out_pct, void *user)
{
    (void)user;
    if (!out_pct) {
        return -1;
    }
    bsp_adc_sample_battery_to_status();
    *out_pct = common_status_get()->battery_soc;
    return 0;
}

static int read_signal_csq(int16_t *out_csq, void *user)
{
    (void)user;
    if (!out_csq) {
        return -1;
    }
    *out_csq = common_status_get()->signal_csq;
    return 0;
}

static int read_storage_free_bytes(uint32_t *out_free, void *user)
{
    (void)user;
    if (!out_free) {
        return -1;
    }
    *out_free = FLASH_STAGING_SLOT_SIZE_BYTES;
    return 0;
}

static int erase_upgrade_region(void *user)
{
    uint32_t addr;

    (void)user;
    for (addr = FLASH_STAGING_SLOT_ADDR;
         addr < FLASH_STAGING_SLOT_END;
         addr += STM32F103_FLASH_PAGE_SIZE_BYTES) {
        if (bsp_flash_erase_sector(addr) != 0) {
            return -1;
        }
    }
    return 0;
}

static int write_upgrade_region(uint32_t offset, const uint8_t *data, size_t len, void *user)
{
    uint32_t addr;

    (void)user;
    if ((data == NULL && len != 0U) || offset > FLASH_STAGING_SLOT_SIZE_BYTES) {
        return -1;
    }
    if ((uint64_t)offset + (uint64_t)len > (uint64_t)FLASH_STAGING_SLOT_SIZE_BYTES) {
        return -1;
    }
    addr = FLASH_STAGING_SLOT_ADDR + offset;
    return bsp_flash_write(addr, data, len);
}

static void reboot_to_new_image(void *user)
{
    ota_prepare_payload_t manifest;

    (void)user;
    if (storage_upgrade_load_manifest(&manifest) != 0) {
        bsp_debug_log("[OTA] reboot rejected: manifest missing\r\n");
        return;
    }
    if (boot_control_schedule_upgrade(manifest.package_size) != 0) {
        bsp_debug_log("[OTA] reboot rejected: boot control write failed\r\n");
        return;
    }

    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
    *(volatile uint32_t *)SCB_AIRCR_ADDR = SCB_AIRCR_SYSRESET;
    __asm volatile("dsb" ::: "memory");
    for (;;) {
    }
}

static const ota_port_t s_port = {
    .tcp_session_stable  = read_tcp_ok,
    .get_battery_soc     = read_battery_soc,
    .get_signal_csq      = read_signal_csq,
    .http_download_chunk = NULL,
    .sha256_init         = NULL,
    .sha256_update       = NULL,
    .sha256_final        = NULL,
    .sha256_free         = NULL,
    .flash_erase_upgrade_region = erase_upgrade_region,
    .flash_write_upgrade_region = write_upgrade_region,
    .reboot_to_new_image = reboot_to_new_image,
    .storage_free_bytes  = read_storage_free_bytes,
    .user                = NULL,
};

const ota_port_t *ota_port_board(void)
{
    return &s_port;
}
