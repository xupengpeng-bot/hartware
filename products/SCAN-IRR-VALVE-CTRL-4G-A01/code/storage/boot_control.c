#include "boot_control.h"

#include "stm32f1_flash.h"

static int boot_control_store_record(const boot_control_record_t *record)
{
    if (record == NULL || !boot_control_record_is_valid(record)) {
        return -1;
    }
    if (stm32f1_flash_erase_page(FLASH_BOOT_CONTROL_PAGE_ADDR) != 0) {
        return -1;
    }
    return stm32f1_flash_program(FLASH_BOOT_CONTROL_PAGE_ADDR, record, sizeof(*record));
}

int boot_control_load(boot_control_record_t *out)
{
    boot_control_record_t record;

    if (out == NULL) {
        return -1;
    }
    if (stm32f1_flash_read(FLASH_BOOT_CONTROL_PAGE_ADDR, &record, sizeof(record)) != 0) {
        return -1;
    }
    if (!boot_control_record_is_valid(&record)) {
        return -1;
    }
    *out = record;
    return 0;
}

int boot_control_schedule_upgrade(uint32_t image_size)
{
    boot_control_record_t record;

    if (image_size == 0u || image_size > FLASH_STAGING_SLOT_SIZE_BYTES || image_size > FLASH_APP_SLOT_SIZE_BYTES) {
        return -1;
    }
    boot_control_record_prepare(&record, BOOT_CONTROL_STATE_STAGED, image_size, 0u);
    return boot_control_store_record(&record);
}

int boot_control_store_copy_progress(uint32_t image_size, uint32_t bytes_copied)
{
    boot_control_record_t record;

    if (image_size == 0u || image_size > FLASH_APP_SLOT_SIZE_BYTES || bytes_copied > image_size) {
        return -1;
    }
    boot_control_record_prepare(&record, BOOT_CONTROL_STATE_COPYING, image_size, bytes_copied);
    return boot_control_store_record(&record);
}

int boot_control_mark_trial_boot(uint32_t image_size)
{
    boot_control_record_t record;

    if (image_size == 0u || image_size > FLASH_APP_SLOT_SIZE_BYTES) {
        return -1;
    }
    boot_control_record_prepare(&record, BOOT_CONTROL_STATE_TRIAL_BOOT, image_size, image_size);
    return boot_control_store_record(&record);
}

int boot_control_clear(void)
{
    return stm32f1_flash_erase_page(FLASH_BOOT_CONTROL_PAGE_ADDR);
}
