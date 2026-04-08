/**
 * Minimal UTF-8 JSON helpers for tcp-json-v1 (no external parser).
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

int proto_json_get_string(const char *json, const char *key, char *out, size_t out_sz);
int proto_json_get_u32(const char *json, const char *key, uint32_t *out);
int proto_json_get_i32(const char *json, const char *key, int32_t *out);
/** Reads 0/1 after key. */
int proto_json_get_u8_01(const char *json, const char *key, uint8_t *out);

#endif /* PROTO_CODEC_JSON_H */
