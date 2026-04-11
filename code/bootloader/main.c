#include "boot_control.h"
#include "flash_layout.h"
#include "stm32f1_flash.h"

#include <stdint.h>

#define SCB_VTOR         (*(volatile uint32_t *)0xE000ED08u)
#define COPY_CHUNK_BYTES 256u

static uint8_t s_copy_buf[COPY_CHUNK_BYTES];

static uint32_t rd32(uint32_t a)
{
    return *(volatile uint32_t *)a;
}

static uint32_t align_up_to_flash_page(uint32_t n)
{
    return (n + (STM32F103_FLASH_PAGE_SIZE_BYTES - 1u)) &
           ~(STM32F103_FLASH_PAGE_SIZE_BYTES - 1u);
}

static int image_header_ok(uint32_t image_addr)
{
    uint32_t sp = rd32(image_addr);
    uint32_t pc = rd32(image_addr + 4u);

    if ((sp & 0xFFFE0000u) != 0x20000000u) {
        return 0;
    }
    if ((pc & 1u) == 0u) {
        return 0;
    }
    pc &= ~1u;
    if (pc < FLASH_APP_SLOT_ADDR || pc >= FLASH_APP_SLOT_END) {
        return 0;
    }
    return 1;
}

static int app_ok(void)
{
    return image_header_ok(FLASH_APP_SLOT_ADDR);
}

static void set_msp(uint32_t sp)
{
    __asm volatile("msr msp, %0" : : "r"(sp) : "memory");
}

static void go_app(void)
{
    uint32_t sp = rd32(FLASH_APP_SLOT_ADDR);
    uint32_t pc = rd32(FLASH_APP_SLOT_ADDR + 4u);
    void (*reset)(void) = (void (*)(void))pc;

    __asm volatile("cpsid i" ::: "memory");
    SCB_VTOR = FLASH_APP_SLOT_ADDR;
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
    set_msp(sp);
    __asm volatile("cpsie i" ::: "memory");
    reset();
}

static int erase_app_region(uint32_t image_size)
{
    uint32_t addr;
    uint32_t total = align_up_to_flash_page(image_size);

    for (addr = FLASH_APP_SLOT_ADDR; addr < FLASH_APP_SLOT_ADDR + total; addr += STM32F103_FLASH_PAGE_SIZE_BYTES) {
        if (stm32f1_flash_erase_page(addr) != 0) {
            return -1;
        }
    }
    return 0;
}

static int copy_image_from_staging(const boot_control_record_t *record)
{
    uint32_t offset;
    uint32_t image_size;

    if (record == NULL || !boot_control_record_is_valid(record)) {
        return -1;
    }
    if (!image_header_ok(FLASH_STAGING_SLOT_ADDR)) {
        return -1;
    }

    image_size = record->image_size;
    offset = record->state == BOOT_CONTROL_STATE_COPYING ? record->bytes_copied : 0u;

    if (record->state != BOOT_CONTROL_STATE_COPYING) {
        if (erase_app_region(image_size) != 0) {
            return -1;
        }
        if (boot_control_store_copy_progress(image_size, 0u) != 0) {
            return -1;
        }
        offset = 0u;
    }

    while (offset < image_size) {
        uint32_t chunk = image_size - offset;
        uint32_t checkpoint;

        if (chunk > COPY_CHUNK_BYTES) {
            chunk = COPY_CHUNK_BYTES;
        }
        if (stm32f1_flash_read(FLASH_STAGING_SLOT_ADDR + offset, s_copy_buf, chunk) != 0) {
            return -1;
        }
        if (stm32f1_flash_program(FLASH_APP_SLOT_ADDR + offset, s_copy_buf, chunk) != 0) {
            return -1;
        }

        offset += chunk;
        checkpoint = offset;
        if ((checkpoint % STM32F103_FLASH_PAGE_SIZE_BYTES) == 0u || checkpoint == image_size) {
            if (boot_control_store_copy_progress(image_size, checkpoint) != 0) {
                return -1;
            }
        }
    }

    if (!app_ok()) {
        return -1;
    }
    return boot_control_mark_trial_boot(image_size);
}

int main(void)
{
    boot_control_record_t record;

    if (boot_control_load(&record) == 0) {
        if (record.state == BOOT_CONTROL_STATE_STAGED || record.state == BOOT_CONTROL_STATE_COPYING) {
            if (copy_image_from_staging(&record) == 0 && app_ok()) {
                go_app();
            }
        } else if (record.state == BOOT_CONTROL_STATE_TRIAL_BOOT) {
            if (!app_ok() && image_header_ok(FLASH_STAGING_SLOT_ADDR)) {
                if (boot_control_schedule_upgrade(record.image_size) == 0 &&
                    boot_control_load(&record) == 0 &&
                    copy_image_from_staging(&record) == 0 &&
                    app_ok()) {
                    go_app();
                }
            } else if (app_ok()) {
                go_app();
            }
        }
    }

    if (app_ok()) {
        go_app();
    }

    for (;;) {
        __asm volatile("wfi");
    }
}
