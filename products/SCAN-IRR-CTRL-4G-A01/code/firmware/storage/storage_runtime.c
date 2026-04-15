#include "storage_runtime.h"
#include "../bsp/bsp_flash.h"

#include <string.h>

#define STORAGE_RUNTIME_MAGIC   0x52544E54UL
#define STORAGE_RUNTIME_VERSION 2UL

#ifndef STORAGE_RUNTIME_FLASH_ADDR
#define STORAGE_RUNTIME_FLASH_ADDR 0UL
#endif

typedef struct {
    uint32_t         magic;
    uint32_t         version;
    uint32_t         payload_size;
    uint32_t         crc32;
    device_runtime_t payload;
} storage_runtime_record_t;

static device_runtime_t s_nv;
static device_runtime_t s_persisted;
static bool             s_dirty;
static bool             s_persisted_valid;

static uint32_t storage_runtime_crc32(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t       crc = 0xFFFFFFFFUL;
    size_t         idx;
    uint8_t        bit;

    if (!bytes) {
        return 0UL;
    }

    for (idx = 0U; idx < len; idx++) {
        crc ^= (uint32_t)bytes[idx];
        for (bit = 0U; bit < 8U; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1UL);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}

static void storage_runtime_refresh_dirty(void)
{
    s_dirty = (bool)(s_nv.active_session.session_id[0] != '\0');
}

static int storage_runtime_record_is_valid(const storage_runtime_record_t *record)
{
    uint32_t expected_crc;

    if (!record) {
        return 0;
    }
    if (record->magic != STORAGE_RUNTIME_MAGIC || record->version != STORAGE_RUNTIME_VERSION) {
        return 0;
    }
    if (record->payload_size != sizeof(record->payload)) {
        return 0;
    }

    expected_crc = storage_runtime_crc32(&record->payload, sizeof(record->payload));
    return expected_crc == record->crc32;
}

static int storage_runtime_try_load_flash(storage_runtime_record_t *record)
{
    if (!record) {
        return -1;
    }
    if (bsp_flash_read(STORAGE_RUNTIME_FLASH_ADDR, record, sizeof(*record)) != 0) {
        return -2;
    }
    if (!storage_runtime_record_is_valid(record)) {
        return -3;
    }
    return 0;
}

static void storage_runtime_build_record(const device_runtime_t *runtime, storage_runtime_record_t *record)
{
    memset(record, 0, sizeof(*record));
    record->magic = STORAGE_RUNTIME_MAGIC;
    record->version = STORAGE_RUNTIME_VERSION;
    record->payload_size = sizeof(record->payload);
    record->payload = *runtime;
    record->crc32 = storage_runtime_crc32(&record->payload, sizeof(record->payload));
}

static int storage_runtime_try_save_flash(const device_runtime_t *runtime)
{
    storage_runtime_record_t record;

    if (!runtime) {
        return -1;
    }

    storage_runtime_build_record(runtime, &record);
    if (bsp_flash_erase_sector(STORAGE_RUNTIME_FLASH_ADDR) != 0) {
        return -2;
    }
    if (bsp_flash_write(STORAGE_RUNTIME_FLASH_ADDR, &record, sizeof(record)) != 0) {
        return -3;
    }
    return 0;
}

void storage_runtime_init(void)
{
    storage_runtime_record_t record;

    memset(&s_nv, 0, sizeof(s_nv));
    memset(&s_persisted, 0, sizeof(s_persisted));
    s_dirty = false;
    s_persisted_valid = false;

    if (storage_runtime_try_load_flash(&record) == 0) {
        s_nv = record.payload;
        s_persisted = record.payload;
        s_persisted_valid = true;
        storage_runtime_refresh_dirty();
        return;
    }
}

int storage_runtime_save(const device_runtime_t *rt)
{
    if (!rt) {
        return -1;
    }

    s_nv = *rt;
    s_persisted = *rt;
    s_persisted_valid = true;
    storage_runtime_refresh_dirty();
    (void)storage_runtime_try_save_flash(rt);
    return 0;
}

int storage_runtime_load(device_runtime_t *out)
{
    if (!out) {
        return -1;
    }
    if (!s_persisted_valid) {
        return -2;
    }
    *out = s_nv;
    return 0;
}

bool storage_runtime_has_dirty_session(void)
{
    return s_dirty && s_nv.active_session.session_id[0] != '\0';
}

void storage_runtime_clear_dirty(void)
{
    memset(&s_nv.active_session, 0, sizeof(s_nv.active_session));
    memset(&s_persisted.active_session, 0, sizeof(s_persisted.active_session));
    storage_runtime_refresh_dirty();
    if (s_persisted_valid) {
        (void)storage_runtime_try_save_flash(&s_persisted);
    }
}
