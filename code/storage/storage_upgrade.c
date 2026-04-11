#include "storage_upgrade.h"

#include "flash_layout.h"
#include "stm32f1_flash.h"

#include <string.h>

#define STORAGE_UPGRADE_MAGIC   0x4F54414Du
#define STORAGE_UPGRADE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    ota_prepare_payload_t manifest;
    uint32_t crc32;
} storage_upgrade_flash_record_t;

typedef char storage_upgrade_record_fits_in_page[
    (sizeof(storage_upgrade_flash_record_t) <= STM32F103_FLASH_PAGE_SIZE_BYTES) ? 1 : -1];

static ota_prepare_payload_t s_manifest;
static bool                  s_has_manifest;

static ota_state_t s_persist_state;
static uint8_t     s_persist_dl_pct;
static uint8_t     s_persist_wr_pct;
static uint32_t    s_download_offset;
static bool        s_state_valid;

static uint32_t storage_upgrade_crc32(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    for (i = 0u; i < len; i++) {
        uint32_t bit;
        crc ^= (uint32_t)bytes[i];
        for (bit = 0u; bit < 8u; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static int storage_upgrade_flash_load(ota_prepare_payload_t *out)
{
    storage_upgrade_flash_record_t record;
    uint32_t crc;

    if (out == NULL) {
        return -1;
    }
    if (stm32f1_flash_read(FLASH_OTA_METADATA_PAGE_ADDR, &record, sizeof(record)) != 0) {
        return -1;
    }
    if (record.magic != STORAGE_UPGRADE_MAGIC || record.version != STORAGE_UPGRADE_VERSION) {
        return -1;
    }
    crc = storage_upgrade_crc32(&record.manifest, sizeof(record.manifest));
    if (crc != record.crc32) {
        return -1;
    }
    *out = record.manifest;
    return 0;
}

static int storage_upgrade_flash_save(const ota_prepare_payload_t *manifest)
{
    storage_upgrade_flash_record_t record;

    if (manifest == NULL) {
        return -1;
    }
    memset(&record, 0, sizeof(record));
    record.magic = STORAGE_UPGRADE_MAGIC;
    record.version = STORAGE_UPGRADE_VERSION;
    record.manifest = *manifest;
    record.crc32 = storage_upgrade_crc32(&record.manifest, sizeof(record.manifest));

    if (stm32f1_flash_erase_page(FLASH_OTA_METADATA_PAGE_ADDR) != 0) {
        return -1;
    }
    return stm32f1_flash_program(FLASH_OTA_METADATA_PAGE_ADDR, &record, sizeof(record));
}

int storage_upgrade_init(void)
{
    if (storage_upgrade_flash_load(&s_manifest) == 0) {
        s_has_manifest = true;
    } else {
        s_has_manifest = false;
        memset(&s_manifest, 0, sizeof(s_manifest));
    }
    s_persist_state = OTA_STATE_IDLE;
    s_persist_dl_pct = 0;
    s_persist_wr_pct = 0;
    s_download_offset = 0;
    s_state_valid = false;
    return 0;
}

int storage_upgrade_save_manifest(const ota_prepare_payload_t *manifest)
{
    if (!manifest) {
        return -1;
    }
    if (storage_upgrade_flash_save(manifest) != 0) {
        return -1;
    }
    s_manifest = *manifest;
    s_has_manifest = true;
    return 0;
}

int storage_upgrade_load_manifest(ota_prepare_payload_t *out)
{
    if (!out || !s_has_manifest) {
        return -1;
    }
    *out = s_manifest;
    return 0;
}

void storage_upgrade_clear_manifest(void)
{
    (void)stm32f1_flash_erase_page(FLASH_OTA_METADATA_PAGE_ADDR);
    s_has_manifest = false;
    memset(&s_manifest, 0, sizeof(s_manifest));
}

bool storage_upgrade_has_manifest(void)
{
    return s_has_manifest;
}

int storage_upgrade_save_ota_state(ota_state_t state, uint8_t download_pct, uint8_t write_pct)
{
    s_persist_state = state;
    s_persist_dl_pct = download_pct;
    s_persist_wr_pct = write_pct;
    s_state_valid = true;
    return 0;
}

int storage_upgrade_load_ota_state(ota_state_t *state, uint8_t *download_pct, uint8_t *write_pct)
{
    if (!state || !download_pct || !write_pct) {
        return -1;
    }
    if (!s_state_valid) {
        return -1;
    }
    *state = s_persist_state;
    *download_pct = s_persist_dl_pct;
    *write_pct = s_persist_wr_pct;
    return 0;
}

int storage_upgrade_save_download_offset(uint32_t offset)
{
    s_download_offset = offset;
    return 0;
}

int storage_upgrade_load_download_offset(uint32_t *offset)
{
    if (!offset) {
        return -1;
    }
    *offset = s_download_offset;
    return 0;
}
