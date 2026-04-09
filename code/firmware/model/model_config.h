/**
 * On-device configuration aggregate — Firmware Dev Spec v1 §10.
 */
#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

#include "model_types.h"

#define MODEL_MAX_CHANNEL_BINDINGS 32U
/** 与 NET_PLATFORM_HOST_MAX 对齐，供 device_config 存平台 TCP 地址（空则使用编译期默认）。 */
#define MODEL_PLATFORM_HOST_MAX 96U

typedef struct {
    uint32_t            config_version;
    feature_modules_t   feature_modules;
    uint16_t            channel_binding_count;
    channel_binding_t   channel_bindings[MODEL_MAX_CHANNEL_BINDINGS];
    runtime_rules_t     runtime_rules;
    /** 运行时覆盖北向 TCP；host 全空且 port==0 表示使用 fw_build_config 默认。 */
    char                platform_tcp_host[MODEL_PLATFORM_HOST_MAX];
    uint16_t            platform_tcp_port;
} device_config_t;

#endif /* MODEL_CONFIG_H */
