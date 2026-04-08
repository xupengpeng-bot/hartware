/**
 * On-device configuration aggregate — Firmware Dev Spec v1 §10.
 */
#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

#include "model_types.h"

#define MODEL_MAX_CHANNEL_BINDINGS 32U

typedef struct {
    uint32_t            config_version;
    feature_modules_t   feature_modules;
    uint16_t            channel_binding_count;
    channel_binding_t   channel_bindings[MODEL_MAX_CHANNEL_BINDINGS];
    runtime_rules_t     runtime_rules;
} device_config_t;

#endif /* MODEL_CONFIG_H */
