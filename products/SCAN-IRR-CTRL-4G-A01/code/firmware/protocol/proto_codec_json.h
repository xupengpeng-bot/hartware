/**
 * Minimal UTF-8 JSON helpers for the compact device protocol.
 *
 * Canonical protocol source:
 * http://xupengpeng.top/ops/interface-protocols#device-protocols
 */
#ifndef PROTO_CODEC_JSON_H
#define PROTO_CODEC_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} json_buf_t;

void json_buf_init(json_buf_t *b, char *mem, size_t cap);
int  json_buf_append(json_buf_t *b, const char *s);
int  json_buf_append_fmt(json_buf_t *b, const char *fmt, ...);
int  json_escape_append(json_buf_t *b, const char *s);
int  json_buf_append_fixed(json_buf_t *b, float value, uint8_t frac_digits);

int proto_json_get_string(const char *json, const char *key, char *out, size_t out_sz);
int proto_json_get_u32(const char *json, const char *key, uint32_t *out);
int proto_json_get_i32(const char *json, const char *key, int32_t *out);
int proto_json_get_float(const char *json, const char *key, float *out);
int proto_json_get_bool(const char *json, const char *key, uint8_t *out);
/** Reads 0/1 after key. */
int proto_json_get_u8_01(const char *json, const char *key, uint8_t *out);

const char *proto_map_reject_short(const char *value);
const char *proto_map_module_short(const char *value);
const char *proto_map_workflow_short(const char *value);
const char *proto_map_power_mode_short(const char *value);
const char *proto_map_workflow_short_from_runtime(const char *value, uint8_t ready_flag);

#endif /* PROTO_CODEC_JSON_H */
