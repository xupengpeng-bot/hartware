#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

#define STM32F103_FLASH_BASE             0x08000000u
#define STM32F103_FLASH_SIZE_BYTES       (256u * 1024u)
#define STM32F103_FLASH_END              (STM32F103_FLASH_BASE + STM32F103_FLASH_SIZE_BYTES)
#define STM32F103_FLASH_PAGE_SIZE_BYTES  2048u

/*
 * Minimal OTA layout for STM32F103RC (256 KiB flash):
 * - bootloader code      : 12 KiB
 * - ota metadata page    :  2 KiB
 * - boot control page    :  2 KiB
 * - application slot     : 120 KiB
 * - staging/download slot: 120 KiB
 */
#define FLASH_BOOTLOADER_ADDR            STM32F103_FLASH_BASE
#define FLASH_BOOTLOADER_SIZE_BYTES      (12u * 1024u)
#define FLASH_OTA_METADATA_PAGE_ADDR     (STM32F103_FLASH_BASE + 0x00003000u)
#define FLASH_BOOT_CONTROL_PAGE_ADDR     (STM32F103_FLASH_BASE + 0x00003800u)

#define FLASH_APP_SLOT_ADDR              (STM32F103_FLASH_BASE + 0x00004000u)
#define FLASH_APP_SLOT_SIZE_BYTES        (120u * 1024u)
#define FLASH_APP_SLOT_END               (FLASH_APP_SLOT_ADDR + FLASH_APP_SLOT_SIZE_BYTES)

#define FLASH_STAGING_SLOT_ADDR          FLASH_APP_SLOT_END
#define FLASH_STAGING_SLOT_SIZE_BYTES    (120u * 1024u)
#define FLASH_STAGING_SLOT_END           (FLASH_STAGING_SLOT_ADDR + FLASH_STAGING_SLOT_SIZE_BYTES)

#if FLASH_STAGING_SLOT_END != STM32F103_FLASH_END
#error "Flash layout does not fill the expected 256 KiB address space"
#endif

#endif /* FLASH_LAYOUT_H */
