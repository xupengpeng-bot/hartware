#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

#define STM32F103_FLASH_BASE             0x08000000u
#define STM32F103_FLASH_SIZE_BYTES       (256u * 1024u)
#define STM32F103_FLASH_END              (STM32F103_FLASH_BASE + STM32F103_FLASH_SIZE_BYTES)
#define STM32F103_FLASH_PAGE_SIZE_BYTES  2048u

/*
 * OTA layout while keeping the historical 16 KiB bootloader reservation:
 * - bootloader code      :  16 KiB  @ 0x08000000
 * - application slot     : 120 KiB  @ 0x08004000
 * - staging/download slot: 116 KiB  @ 0x08022000
 * - ota metadata page    :   2 KiB  @ 0x0803F000
 * - boot control page    :   2 KiB  @ 0x0803F800
 */
#define FLASH_BOOTLOADER_ADDR            STM32F103_FLASH_BASE
#define FLASH_BOOTLOADER_SIZE_BYTES      (16u * 1024u)

#define FLASH_APP_SLOT_ADDR              (STM32F103_FLASH_BASE + FLASH_BOOTLOADER_SIZE_BYTES)
#define FLASH_APP_SLOT_SIZE_BYTES        (120u * 1024u)
#define FLASH_APP_SLOT_END               (FLASH_APP_SLOT_ADDR + FLASH_APP_SLOT_SIZE_BYTES)

#define FLASH_STAGING_SLOT_ADDR          FLASH_APP_SLOT_END
#define FLASH_STAGING_SLOT_SIZE_BYTES    (116u * 1024u)
#define FLASH_STAGING_SLOT_END           (FLASH_STAGING_SLOT_ADDR + FLASH_STAGING_SLOT_SIZE_BYTES)

#define FLASH_OTA_METADATA_PAGE_ADDR     FLASH_STAGING_SLOT_END
#define FLASH_BOOT_CONTROL_PAGE_ADDR     (FLASH_OTA_METADATA_PAGE_ADDR + STM32F103_FLASH_PAGE_SIZE_BYTES)

#if (FLASH_BOOT_CONTROL_PAGE_ADDR + STM32F103_FLASH_PAGE_SIZE_BYTES) != STM32F103_FLASH_END
#error "Flash layout does not fill the expected 256 KiB address space"
#endif

#endif /* FLASH_LAYOUT_H */
