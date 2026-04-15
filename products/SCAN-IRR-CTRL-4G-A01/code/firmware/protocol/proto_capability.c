#include "proto_capability.h"

#include "common_identity.h"
#include "ota_port_board.h"
#include "proto_ota.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t state[8];
    uint64_t total_len;
    uint8_t  block[64];
    size_t   block_len;
} capability_sha256_ctx_t;

enum {
    CAP_CONFIG_FEATURE_MODULES = (1UL << 0),
    CAP_CONFIG_RUNTIME_RULES   = (1UL << 1),
    CAP_CONFIG_PROTECTION      = (1UL << 2),
    CAP_CONFIG_CONTROL         = (1UL << 3),

    CAP_ACTION_START_PUMP      = (1UL << 0),
    CAP_ACTION_STOP_PUMP       = (1UL << 1),
    CAP_ACTION_OPEN_VALVE      = (1UL << 2),
    CAP_ACTION_CLOSE_VALVE     = (1UL << 3),
    CAP_ACTION_PAUSE_SESSION   = (1UL << 4),
    CAP_ACTION_RESUME_SESSION  = (1UL << 5),
    CAP_ACTION_UPGRADE         = (1UL << 6),
    CAP_ACTION_REMOTE_REBOOT   = (1UL << 7),
    CAP_ACTION_OPEN_RELAY      = (1UL << 8),
    CAP_ACTION_CLOSE_RELAY     = (1UL << 9),

    CAP_QUERY_COMMON_STATUS    = (1UL << 0),
    CAP_QUERY_WORKFLOW_STATE   = (1UL << 1),
    CAP_QUERY_ELECTRIC_METER   = (1UL << 2),
    CAP_QUERY_UPGRADE_STATUS   = (1UL << 3),
    CAP_QUERY_UPGRADE_CAP      = (1UL << 4)
};

static uint32_t sha256_rotr32(uint32_t value, uint8_t bits)
{
    return (value >> bits) | (value << (32U - bits));
}

static void sha256_transform(capability_sha256_ctx_t *ctx, const uint8_t block[64])
{
    static const uint32_t k[64] = {
        0x428A2F98UL, 0x71374491UL, 0xB5C0FBCFUL, 0xE9B5DBA5UL,
        0x3956C25BUL, 0x59F111F1UL, 0x923F82A4UL, 0xAB1C5ED5UL,
        0xD807AA98UL, 0x12835B01UL, 0x243185BEUL, 0x550C7DC3UL,
        0x72BE5D74UL, 0x80DEB1FEUL, 0x9BDC06A7UL, 0xC19BF174UL,
        0xE49B69C1UL, 0xEFBE4786UL, 0x0FC19DC6UL, 0x240CA1CCUL,
        0x2DE92C6FUL, 0x4A7484AAUL, 0x5CB0A9DCUL, 0x76F988DAUL,
        0x983E5152UL, 0xA831C66DUL, 0xB00327C8UL, 0xBF597FC7UL,
        0xC6E00BF3UL, 0xD5A79147UL, 0x06CA6351UL, 0x14292967UL,
        0x27B70A85UL, 0x2E1B2138UL, 0x4D2C6DFCUL, 0x53380D13UL,
        0x650A7354UL, 0x766A0ABBUL, 0x81C2C92EUL, 0x92722C85UL,
        0xA2BFE8A1UL, 0xA81A664BUL, 0xC24B8B70UL, 0xC76C51A3UL,
        0xD192E819UL, 0xD6990624UL, 0xF40E3585UL, 0x106AA070UL,
        0x19A4C116UL, 0x1E376C08UL, 0x2748774CUL, 0x34B0BCB5UL,
        0x391C0CB3UL, 0x4ED8AA4AUL, 0x5B9CCA4FUL, 0x682E6FF3UL,
        0x748F82EEUL, 0x78A5636FUL, 0x84C87814UL, 0x8CC70208UL,
        0x90BEFFFAUL, 0xA4506CEBUL, 0xBEF9A3F7UL, 0xC67178F2UL
    };
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    size_t i;

    for (i = 0U; i < 16U; i++) {
        size_t offset = i * 4U;
        w[i] = ((uint32_t)block[offset] << 24) |
               ((uint32_t)block[offset + 1U] << 16) |
               ((uint32_t)block[offset + 2U] << 8) |
               (uint32_t)block[offset + 3U];
    }
    for (i = 16U; i < 64U; i++) {
        uint32_t s0 = sha256_rotr32(w[i - 15U], 7U) ^ sha256_rotr32(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
        uint32_t s1 = sha256_rotr32(w[i - 2U], 17U) ^ sha256_rotr32(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0U; i < 64U; i++) {
        uint32_t s1 = sha256_rotr32(e, 6U) ^ sha256_rotr32(e, 11U) ^ sha256_rotr32(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t s0 = sha256_rotr32(a, 2U) ^ sha256_rotr32(a, 13U) ^ sha256_rotr32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(capability_sha256_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6A09E667UL;
    ctx->state[1] = 0xBB67AE85UL;
    ctx->state[2] = 0x3C6EF372UL;
    ctx->state[3] = 0xA54FF53AUL;
    ctx->state[4] = 0x510E527FUL;
    ctx->state[5] = 0x9B05688CUL;
    ctx->state[6] = 0x1F83D9ABUL;
    ctx->state[7] = 0x5BE0CD19UL;
}

static void sha256_update(capability_sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i;

    if (ctx == NULL || data == NULL || len == 0U) {
        return;
    }

    ctx->total_len += (uint64_t)len;
    for (i = 0U; i < len; i++) {
        ctx->block[ctx->block_len++] = data[i];
        if (ctx->block_len == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->block_len = 0U;
        }
    }
}

static void sha256_final(capability_sha256_ctx_t *ctx, uint8_t digest[32])
{
    uint64_t total_bits;
    size_t i;

    if (ctx == NULL || digest == NULL) {
        return;
    }

    total_bits = ctx->total_len * 8ULL;
    ctx->block[ctx->block_len++] = 0x80U;
    if (ctx->block_len > 56U) {
        while (ctx->block_len < 64U) {
            ctx->block[ctx->block_len++] = 0U;
        }
        sha256_transform(ctx, ctx->block);
        ctx->block_len = 0U;
    }
    while (ctx->block_len < 56U) {
        ctx->block[ctx->block_len++] = 0U;
    }
    for (i = 0U; i < 8U; i++) {
        ctx->block[56U + i] = (uint8_t)(total_bits >> (56U - (i * 8U)));
    }
    sha256_transform(ctx, ctx->block);

    for (i = 0U; i < 8U; i++) {
        digest[i * 4U] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4U + 1U] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4U + 2U] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4U + 3U] = (uint8_t)(ctx->state[i]);
    }
}

static uint8_t pump_control_available(const feature_modules_t *fm)
{
    if (fm == NULL) {
        return 0U;
    }
    return (fm->pump_vfd_control != 0U ||
            fm->pump_direct_control != 0U ||
            fm->breaker_control != 0U) ? 1U : 0U;
}

static uint8_t valve_control_available(const feature_modules_t *fm)
{
    return (fm != NULL && fm->dual_valve_control != 0U) ? 1U : 0U;
}

static uint8_t relay_control_available(const feature_modules_t *fm)
{
    return (fm != NULL && fm->relay_output_control != 0U) ? 1U : 0U;
}

static uint8_t workflow_control_available(const device_config_t *cfg, const feature_modules_t *fm)
{
    if (cfg == NULL || pump_control_available(fm) == 0U) {
        return 0U;
    }
    return cfg->runtime_rules.workflow_enabled != 0U ? 1U : 0U;
}

static uint8_t remote_reboot_available(void)
{
    const ota_port_t *ota_port = ota_port_board();

    return (ota_port != NULL && ota_port->reboot_mcu != NULL) ? 1U : 0U;
}

static uint32_t describe_config_bitmap(void)
{
    return CAP_CONFIG_FEATURE_MODULES |
           CAP_CONFIG_RUNTIME_RULES |
           CAP_CONFIG_PROTECTION |
           CAP_CONFIG_CONTROL;
}

static uint32_t describe_actions_bitmap(const device_config_t *cfg, const feature_modules_t *fm)
{
    const ota_upgrade_capability_t *ota_cap = proto_ota_get_capability();
    uint32_t bitmap = 0U;

    if (pump_control_available(fm) != 0U) {
        bitmap |= CAP_ACTION_START_PUMP | CAP_ACTION_STOP_PUMP;
    }
    if (valve_control_available(fm) != 0U) {
        bitmap |= CAP_ACTION_OPEN_VALVE | CAP_ACTION_CLOSE_VALVE;
    }
    if (relay_control_available(fm) != 0U) {
        bitmap |= CAP_ACTION_OPEN_RELAY | CAP_ACTION_CLOSE_RELAY;
    }
    if (workflow_control_available(cfg, fm) != 0U) {
        bitmap |= CAP_ACTION_PAUSE_SESSION | CAP_ACTION_RESUME_SESSION;
    }
    if (ota_cap != NULL && ota_cap->ota_supported) {
        bitmap |= CAP_ACTION_UPGRADE;
    }
    if (remote_reboot_available() != 0U) {
        bitmap |= CAP_ACTION_REMOTE_REBOOT;
    }
    return bitmap;
}

static uint32_t describe_queries_bitmap(const device_config_t *cfg, const feature_modules_t *fm)
{
    const ota_upgrade_capability_t *ota_cap = proto_ota_get_capability();
    uint32_t bitmap = CAP_QUERY_COMMON_STATUS;

    if (workflow_control_available(cfg, fm) != 0U) {
        bitmap |= CAP_QUERY_WORKFLOW_STATE;
    }
    if (fm != NULL && fm->electric_meter_modbus != 0U) {
        bitmap |= CAP_QUERY_ELECTRIC_METER;
    }
    if (ota_cap != NULL && ota_cap->ota_supported) {
        bitmap |= CAP_QUERY_UPGRADE_STATUS | CAP_QUERY_UPGRADE_CAP;
    }
    return bitmap;
}

static int format_hex_bitmap(uint32_t value, char out[PROTO_CAPABILITY_BITMAP_HEX_LEN])
{
    int wrote;

    if (out == NULL) {
        return -1;
    }
    wrote = snprintf(out, PROTO_CAPABILITY_BITMAP_HEX_LEN, "0x%08lx", (unsigned long)value);
    return (wrote > 0 && wrote < (int)PROTO_CAPABILITY_BITMAP_HEX_LEN) ? 0 : -1;
}

static int format_limits_json(char out[PROTO_CAPABILITY_LIMITS_JSON_LEN])
{
    const ota_upgrade_capability_t *ota_cap = proto_ota_get_capability();
    int wrote;

    if (out == NULL) {
        return -1;
    }
    wrote = snprintf(out, PROTO_CAPABILITY_LIMITS_JSON_LEN,
                     "{\"max_inflight_control\":1,\"control_queue_depth\":0,"
                     "\"max_query_inflight\":1,\"event_queue_depth\":1,"
                     "\"ota_block_bytes\":256,\"max_channel_bindings\":%u,"
                     "\"ota_dual_bank\":%u}",
                     (unsigned)MODEL_MAX_CHANNEL_BINDINGS,
                     (ota_cap != NULL && ota_cap->dual_bank) ? 1U : 0U);
    return (wrote > 0 && wrote < (int)PROTO_CAPABILITY_LIMITS_JSON_LEN) ? 0 : -1;
}

static int build_capability_hash(const device_config_t *cfg, const feature_modules_t *fm,
                                 uint32_t config_bitmap, uint32_t actions_bitmap,
                                 uint32_t queries_bitmap, const char *limits_json,
                                 char out[PROTO_CAPABILITY_HASH_LEN])
{
    capability_sha256_ctx_t ctx;
    const resource_inventory_t *ri = common_resource_inventory_get();
    uint8_t digest[32];
    char canonical[448];
    int wrote;

    if (cfg == NULL || fm == NULL || limits_json == NULL || out == NULL || ri == NULL) {
        return -1;
    }

    wrote = snprintf(canonical, sizeof(canonical),
                     "cap_ver=%lu;cfg=%08lx;act=%08lx;qry=%08lx;limits=%s;"
                     "fm=%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u;"
                     "ri=%u%u%u%u%u%u%u%u;"
                     "wf=%u",
                     (unsigned long)PROTO_CAPABILITY_VERSION,
                     (unsigned long)config_bitmap,
                     (unsigned long)actions_bitmap,
                     (unsigned long)queries_bitmap,
                     limits_json,
                     fm->payment_qr_control,
                     fm->card_auth_reader,
                     fm->electric_meter_modbus,
                     fm->breaker_control,
                     fm->breaker_feedback_monitor,
                     fm->power_monitoring,
                     fm->pressure_acquisition,
                     fm->flow_acquisition,
                     fm->level_acquisition,
                     fm->soil_moisture_acquisition,
                     fm->soil_temperature_acquisition,
                     fm->pump_vfd_control,
                     fm->single_valve_control,
                     fm->dual_valve_control,
                     fm->relay_output_control,
                     fm->pump_direct_control,
                     fm->rs485_sensor_gateway,
                     fm->rs485_vfd_gateway,
                     fm->valve_feedback_monitor,
                     fm->pump_fault_feedback,
                     fm->remote_start_enable,
                     fm->auto_linkage_enable,
                     fm->auto_stop_on_low_pressure,
                     fm->auto_stop_on_high_pressure,
                     ri->relay_output,
                     ri->motor_driver,
                     ri->digital_input,
                     ri->analog_input,
                     ri->pulse_input,
                     ri->rs485_modbus,
                     ri->power_monitor,
                     ri->card_reader,
                     cfg->runtime_rules.workflow_enabled != 0U ? 1U : 0U);
    if (wrote <= 0 || wrote >= (int)sizeof(canonical)) {
        return -1;
    }

    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)canonical, (size_t)wrote);
    sha256_final(&ctx, digest);

    wrote = snprintf(out, PROTO_CAPABILITY_HASH_LEN,
                     "sha256:%02x%02x%02x%02x%02x%02x%02x%02x",
                     digest[0], digest[1], digest[2], digest[3],
                     digest[4], digest[5], digest[6], digest[7]);
    return (wrote > 0 && wrote < (int)PROTO_CAPABILITY_HASH_LEN) ? 0 : -1;
}

int proto_capability_describe(const device_config_t *cfg, const feature_modules_t *fm,
                              proto_capability_info_t *out)
{
    if (cfg == NULL || fm == NULL || out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->capability_version = PROTO_CAPABILITY_VERSION;
    out->config_bitmap = describe_config_bitmap();
    out->actions_bitmap = describe_actions_bitmap(cfg, fm);
    out->queries_bitmap = describe_queries_bitmap(cfg, fm);

    if (format_hex_bitmap(out->config_bitmap, out->config_bitmap_hex) != 0 ||
        format_hex_bitmap(out->actions_bitmap, out->actions_bitmap_hex) != 0 ||
        format_hex_bitmap(out->queries_bitmap, out->queries_bitmap_hex) != 0 ||
        format_limits_json(out->limits_json) != 0 ||
        build_capability_hash(cfg, fm, out->config_bitmap, out->actions_bitmap,
                              out->queries_bitmap, out->limits_json, out->capability_hash) != 0) {
        return -2;
    }

    return 0;
}
