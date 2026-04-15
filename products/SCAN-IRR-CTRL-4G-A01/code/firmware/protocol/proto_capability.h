#ifndef PROTO_CAPABILITY_H
#define PROTO_CAPABILITY_H

#include "model_config.h"

#include <stdint.h>

#define PROTO_CAPABILITY_VERSION            6U
#define PROTO_CAPABILITY_HASH_LEN           24U
#define PROTO_CAPABILITY_BITMAP_HEX_LEN     11U
#define PROTO_CAPABILITY_LIMITS_JSON_LEN   224U

typedef struct {
    uint32_t capability_version;
    uint32_t config_bitmap;
    uint32_t actions_bitmap;
    uint32_t queries_bitmap;
    char capability_hash[PROTO_CAPABILITY_HASH_LEN];
    char config_bitmap_hex[PROTO_CAPABILITY_BITMAP_HEX_LEN];
    char actions_bitmap_hex[PROTO_CAPABILITY_BITMAP_HEX_LEN];
    char queries_bitmap_hex[PROTO_CAPABILITY_BITMAP_HEX_LEN];
    char limits_json[PROTO_CAPABILITY_LIMITS_JSON_LEN];
} proto_capability_info_t;

int proto_capability_describe(const device_config_t *cfg, const feature_modules_t *fm,
                              proto_capability_info_t *out);

#endif /* PROTO_CAPABILITY_H */
