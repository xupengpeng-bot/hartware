#include "storage_upgrade.h"
#include <string.h>

static ota_prepare_payload_t s_manifest;
static bool                  s_has_manifest;

static ota_state_t s_persist_state;
static uint8_t     s_persist_dl_pct;
static uint8_t     s_persist_wr_pct;
static uint32_t    s_download_offset;
static bool        s_state_valid;

int storage_upgrade_init(void)
{
    s_has_manifest = false;
    memset(&s_manifest, 0, sizeof(s_manifest));
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
