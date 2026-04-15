/**
 * Module ops table — Firmware Dev Spec v1 §8.1.
 */
#ifndef MODULE_REGISTRY_H
#define MODULE_REGISTRY_H

#include "model_types.h"
#include <stddef.h>
#include <stdint.h>

#define MODULE_REGISTRY_MAX 16U

void module_registry_init(void);

uint8_t module_registry_register(const module_ops_t *ops);

const module_ops_t *module_registry_get(const char *module_code);

void module_registry_init_all(void);
void module_registry_tick_100ms_all(void);
void module_registry_tick_1s_all(void);

/** Register all built-in modules (pump, valve, sensors, meter). */
void module_registry_register_builtin(void);

#endif /* MODULE_REGISTRY_H */
