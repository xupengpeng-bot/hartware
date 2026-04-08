#include "proto_codec_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void json_buf_init(json_buf_t *b, char *mem, size_t cap)
{
    b->buf = mem;
    b->cap = cap;
    b->len = 0U;
    if (cap > 0U && mem) {
        mem[0] = '\0';
    }
}

int json_buf_append(json_buf_t *b, const char *s)
{
    if (!b || !b->buf || !s) {
        return -1;
    }
    size_t sl = strlen(s);
    if (b->len + sl + 1U > b->cap) {
        return -2;
    }
    memcpy(b->buf + b->len, s, sl + 1U);
    b->len += sl;
    return 0;
}

int json_buf_append_fmt(json_buf_t *b, const char *fmt, ...)
{
    if (!b || !b->buf || !fmt) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap > b->len ? b->cap - b->len : 0, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= (b->cap > b->len ? b->cap - b->len : 0)) {
        return -2;
    }
    b->len += (size_t)n;
    return 0;
}

int json_escape_append(json_buf_t *b, const char *s)
{
    if (!s) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        char esc[8];
        if (*p == '"' || *p == '\\') {
            esc[0] = '\\';
            esc[1] = (char)*p;
            esc[2] = '\0';
            if (json_buf_append(b, esc) != 0) {
                return -1;
            }
        } else if (*p < 0x20U) {
            (void)snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)*p);
            if (json_buf_append(b, esc) != 0) {
                return -1;
            }
        } else {
            esc[0] = (char)*p;
            esc[1] = '\0';
            if (json_buf_append(b, esc) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int proto_json_get_string(const char *json, const char *key, char *out, size_t out_sz)
{
    if (!json || !key || !out || out_sz == 0U) {
        return -1;
    }
    char pattern[48];
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    p++;
    size_t i = 0U;
    while (*p != '\0' && *p != '"' && i + 1U < out_sz) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

int proto_json_get_u32(const char *json, const char *key, uint32_t *out)
{
    if (!json || !key || !out) {
        return -1;
    }
    char pattern[48];
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    *out = (uint32_t)strtoul(p, NULL, 10);
    return 0;
}

int proto_json_get_i32(const char *json, const char *key, int32_t *out)
{
    if (!json || !key || !out) {
        return -1;
    }
    char pattern[48];
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    *out = (int32_t)strtol(p, NULL, 10);
    return 0;
}

int proto_json_get_u8_01(const char *json, const char *key, uint8_t *out)
{
    uint32_t v = 0U;
    if (proto_json_get_u32(json, key, &v) != 0) {
        return -1;
    }
    *out = (v != 0U) ? 1U : 0U;
    return 0;
}
