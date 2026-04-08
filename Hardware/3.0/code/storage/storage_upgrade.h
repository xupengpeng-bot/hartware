/**
 * storage_upgrade — manifest and OTA intermediate state persistence (Spec §14).
 */
#ifndef STORAGE_UPGRADE_H
#define STORAGE_UPGRADE_H

#include "ota_types.h"
#include <stdbool.h>

int storage_upgrade_init(void);

int storage_upgrade_save_manifest(const ota_prepare_payload_t *manifest);
int storage_upgrade_load_manifest(ota_prepare_payload_t *out);
void storage_upgrade_clear_manifest(void);
bool storage_upgrade_has_manifest(void);

int storage_upgrade_save_ota_state(ota_state_t state, uint8_t download_pct, uint8_t write_pct);
int storage_upgrade_load_ota_state(ota_state_t *state, uint8_t *download_pct, uint8_t *write_pct);

/** Persist download offset for resume (Phase 2); Phase 1 may ignore. */
int storage_upgrade_save_download_offset(uint32_t offset);
int storage_upgrade_load_download_offset(uint32_t *offset);

#endif /* STORAGE_UPGRADE_H */
