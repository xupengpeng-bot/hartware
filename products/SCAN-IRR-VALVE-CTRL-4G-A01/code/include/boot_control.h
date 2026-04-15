#ifndef BOOT_CONTROL_H
#define BOOT_CONTROL_H

#include "flash_layout.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define BOOT_CONTROL_MAGIC    0x42544331u
#define BOOT_CONTROL_VERSION  1u

typedef enum {
    BOOT_CONTROL_STATE_STAGED = 1u,
    BOOT_CONTROL_STATE_COPYING = 2u,
    BOOT_CONTROL_STATE_TRIAL_BOOT = 3u,
} boot_control_state_t;

typedef struct {
    uint32_t magic;
    uint32_t magic_inv;
    uint32_t version;
    uint32_t state;
    uint32_t image_size;
    uint32_t bytes_copied;
    uint32_t reserved0;
    uint32_t reserved1;
} boot_control_record_t;

static inline bool boot_control_state_is_valid(uint32_t state)
{
    return state == BOOT_CONTROL_STATE_STAGED ||
           state == BOOT_CONTROL_STATE_COPYING ||
           state == BOOT_CONTROL_STATE_TRIAL_BOOT;
}

static inline void boot_control_record_prepare(boot_control_record_t *record,
                                               boot_control_state_t state,
                                               uint32_t image_size,
                                               uint32_t bytes_copied)
{
    if (record == NULL) {
        return;
    }
    record->magic = BOOT_CONTROL_MAGIC;
    record->magic_inv = ~BOOT_CONTROL_MAGIC;
    record->version = BOOT_CONTROL_VERSION;
    record->state = (uint32_t)state;
    record->image_size = image_size;
    record->bytes_copied = bytes_copied;
    record->reserved0 = 0u;
    record->reserved1 = 0u;
}

static inline bool boot_control_record_is_valid(const boot_control_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    if (record->magic != BOOT_CONTROL_MAGIC || record->magic_inv != ~BOOT_CONTROL_MAGIC) {
        return false;
    }
    if (record->version != BOOT_CONTROL_VERSION || !boot_control_state_is_valid(record->state)) {
        return false;
    }
    if (record->image_size == 0u || record->image_size > FLASH_STAGING_SLOT_SIZE_BYTES) {
        return false;
    }
    if (record->image_size > FLASH_APP_SLOT_SIZE_BYTES) {
        return false;
    }
    if (record->bytes_copied > record->image_size) {
        return false;
    }
    return true;
}

int boot_control_load(boot_control_record_t *out);
int boot_control_schedule_upgrade(uint32_t image_size);
int boot_control_store_copy_progress(uint32_t image_size, uint32_t bytes_copied);
int boot_control_mark_trial_boot(uint32_t image_size);
int boot_control_clear(void);

#endif /* BOOT_CONTROL_H */
