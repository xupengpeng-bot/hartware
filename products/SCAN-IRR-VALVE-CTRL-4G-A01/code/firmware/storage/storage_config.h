/**
 * Dual-buffer config persistence — Firmware Dev Spec v1 §10.2 (shell; flash backend plugs in later).
 */
#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include "model_config.h"
#include <stdbool.h>
#include <stdint.h>

void storage_config_init(void);

/** Load active config into `out`. Returns 0, or negative if none / corrupt. */
int storage_config_load(device_config_t *out);

/** Write candidate to inactive slot (steps 1–4 of spec). */
int storage_config_stage_inactive(const device_config_t *cfg);

/** Validate inactive and swap active pointer (steps 5–6). */
int storage_config_commit_swap(uint32_t new_version);

bool storage_config_has_valid(void);

uint8_t storage_config_active_slot(void);

/** Return active config pointer, or NULL when no valid config is staged. */
const device_config_t *storage_config_active(void);

/** Return inactive slot pointer cleared for in-place editing. */
device_config_t *storage_config_inactive_mutable(void);

#endif /* STORAGE_CONFIG_H */
