#include "ota_port_board.h"

#include "bsp_adc.h"
#include "bsp_flash.h"
#include "bsp_system.h"
#include "bsp_uart.h"
#include "boot_control.h"
#include "common_status.h"
#include "flash_layout.h"
#include "net_4g_modem.h"
#include "net_connectivity.h"
#include "storage_upgrade.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SCB_AIRCR_ADDR                0xE000ED0Cu
#define SCB_AIRCR_SYSRESET            0x05FA0004u

#define OTA_HTTP_URL_MAX              256U
#define OTA_HTTP_HOST_MAX             96U
#define OTA_HTTP_PATH_MAX             192U
#define OTA_HTTP_HEADER_MAX           512U
#define OTA_HTTP_TEMP_RX_MAX          256U
#define OTA_HTTP_SPILL_MAX            256U
#define OTA_HTTP_BODY_WAIT_MS         8000U
#define OTA_HTTP_POLL_STEP_MS         20U

typedef struct {
    char     host[OTA_HTTP_HOST_MAX];
    char     path[OTA_HTTP_PATH_MAX];
    uint16_t port;
} ota_http_url_t;

typedef struct {
    uint8_t active;
    uint8_t paused_network;
    uint8_t headers_done;
    uint8_t eof;
    uint8_t has_content_length;
    char    url[OTA_HTTP_URL_MAX];
    ota_http_url_t endpoint;
    uint32_t request_offset;
    uint32_t delivered_offset;
    uint32_t received_offset;
    uint32_t content_length;
    uint32_t poll_clock_ms;
    uint8_t  spill[OTA_HTTP_SPILL_MAX];
    size_t   spill_pos;
    size_t   spill_len;
    char     header_buf[OTA_HTTP_HEADER_MAX];
    size_t   header_len;
} ota_http_session_t;

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  block[64];
    size_t   block_len;
} ota_sha256_ctx_t;

static ota_http_session_t s_http;

static bool read_tcp_ok(void *user)
{
    (void)user;
    return common_status_get()->tcp_connected;
}

static int read_battery_soc(uint8_t *out_pct, void *user)
{
    (void)user;
    if (out_pct == NULL) {
        return -1;
    }
    bsp_adc_sample_battery_to_status();
    *out_pct = common_status_get()->battery_soc;
    return 0;
}

static int read_signal_csq(int16_t *out_csq, void *user)
{
    (void)user;
    if (out_csq == NULL) {
        return -1;
    }
    *out_csq = common_status_get()->signal_csq;
    return 0;
}

static int read_storage_free_bytes(uint32_t *out_free, void *user)
{
    (void)user;
    if (out_free == NULL) {
        return -1;
    }
    *out_free = FLASH_STAGING_SLOT_SIZE_BYTES;
    return 0;
}

static size_t ota_local_strnlen(const char *s, size_t max_len)
{
    size_t i;

    for (i = 0U; i < max_len && s[i] != '\0'; i++) {
    }
    return i;
}

static int ota_copy_text(char *dst, size_t cap, const char *src)
{
    size_t n;

    if (dst == NULL || cap == 0U || src == NULL) {
        return -1;
    }
    n = ota_local_strnlen(src, cap);
    if (n >= cap) {
        return -1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
    return 0;
}

static int ota_same_text(const char *a, const char *b)
{
    size_t ai;

    if (a == NULL || b == NULL) {
        return 0;
    }
    for (ai = 0U;; ai++) {
        if (a[ai] != b[ai]) {
            return 0;
        }
        if (a[ai] == '\0') {
            return 1;
        }
    }
}

static void ota_http_close_session(void)
{
    if (net_4g_modem_tcp_is_connected() != 0) {
        net_4g_modem_tcp_close();
    }
    s_http.active = 0U;
    s_http.headers_done = 0U;
    s_http.eof = 0U;
    s_http.has_content_length = 0U;
    s_http.spill_pos = 0U;
    s_http.spill_len = 0U;
    s_http.header_len = 0U;
}

static void ota_http_resume_network(void)
{
    if (s_http.paused_network == 0U) {
        return;
    }
    net_connectivity_resume_after_ota();
    s_http.paused_network = 0U;
}

static int ota_http_pause_network(void)
{
    if (s_http.paused_network != 0U) {
        return 0;
    }
    net_connectivity_pause_for_ota();
    s_http.paused_network = 1U;
    return 0;
}

static int ota_http_parse_url(const char *url, ota_http_url_t *out)
{
    const char *host;
    const char *path;
    const char *port_sep;
    const char *host_end;
    size_t host_len;
    size_t path_len;

    if (url == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (strncmp(url, "http://", 7U) != 0) {
        return -1;
    }
    host = url + 7U;
    path = strchr(host, '/');
    host_end = path != NULL ? path : host + strlen(host);
    port_sep = NULL;
    for (const char *p = host; p < host_end; ++p) {
        if (*p == ':') {
            port_sep = p;
            break;
        }
    }

    if (port_sep != NULL) {
        unsigned long port = 0UL;
        host_len = (size_t)(port_sep - host);
        for (const char *p = port_sep + 1; p < host_end; ++p) {
            if (*p < '0' || *p > '9') {
                return -1;
            }
            port = (port * 10UL) + (unsigned long)(*p - '0');
            if (port > 65535UL) {
                return -1;
            }
        }
        out->port = (uint16_t)port;
    } else {
        host_len = (size_t)(host_end - host);
        out->port = 80U;
    }

    if (host_len == 0U || host_len >= sizeof(out->host)) {
        return -1;
    }
    memcpy(out->host, host, host_len);
    out->host[host_len] = '\0';

    if (path == NULL) {
        out->path[0] = '/';
        out->path[1] = '\0';
        return 0;
    }
    path_len = strlen(path);
    if (path_len == 0U || path_len >= sizeof(out->path)) {
        return -1;
    }
    memcpy(out->path, path, path_len + 1U);
    return 0;
}

static int ota_http_find_header_end(const char *buf, size_t len, size_t *header_len)
{
    size_t i;

    if (buf == NULL || header_len == NULL) {
        return -1;
    }
    for (i = 0U; i + 3U < len; i++) {
        if (buf[i] == '\r' && buf[i + 1U] == '\n' &&
            buf[i + 2U] == '\r' && buf[i + 3U] == '\n') {
            *header_len = i + 4U;
            return 0;
        }
    }
    return -1;
}

static int ota_http_parse_headers(const char *buf, size_t len, uint32_t request_offset, uint32_t *content_length)
{
    int status = 0;
    const char *p;

    if (buf == NULL || content_length == NULL) {
        return -1;
    }
    if (sscanf(buf, "HTTP/%*u.%*u %d", &status) != 1) {
        return -1;
    }
    if (status != 200 && status != 206) {
        return -1;
    }
    if (request_offset > 0U && status != 206) {
        return -1;
    }

    *content_length = 0U;
    p = buf;
    while (p < buf + len) {
        const char *line_end = strstr(p, "\r\n");
        if (line_end == NULL) {
            break;
        }
        if ((size_t)(line_end - p) == 0U) {
            break;
        }
        if ((line_end - p) >= 15 &&
            strncmp(p, "Content-Length:", 15U) == 0) {
            unsigned long value = 0UL;
            const char *q = p + 15U;
            while (q < line_end && (*q == ' ' || *q == '\t')) {
                q++;
            }
            while (q < line_end && *q >= '0' && *q <= '9') {
                value = (value * 10UL) + (unsigned long)(*q - '0');
                if (value > 0xFFFFFFFFUL) {
                    return -1;
                }
                q++;
            }
            *content_length = (uint32_t)value;
        }
        p = line_end + 2U;
    }
    return 0;
}

static void ota_http_note_body_bytes(size_t bytes)
{
    s_http.received_offset += (uint32_t)bytes;
    if (s_http.has_content_length != 0U &&
        s_http.received_offset >= s_http.request_offset + s_http.content_length) {
        s_http.eof = 1U;
        if (net_4g_modem_tcp_is_connected() != 0) {
            net_4g_modem_tcp_close();
        }
    }
}

static int ota_http_copy_spill(uint8_t *out, size_t cap, size_t *produced)
{
    size_t available;
    size_t take;

    if (out == NULL || produced == NULL) {
        return -1;
    }
    while (*produced < cap && s_http.spill_pos < s_http.spill_len) {
        available = s_http.spill_len - s_http.spill_pos;
        take = cap - *produced;
        if (take > available) {
            take = available;
        }
        memcpy(out + *produced, s_http.spill + s_http.spill_pos, take);
        s_http.spill_pos += take;
        *produced += take;
        s_http.delivered_offset += (uint32_t)take;
    }
    if (s_http.spill_pos >= s_http.spill_len) {
        s_http.spill_pos = 0U;
        s_http.spill_len = 0U;
    }
    return 0;
}

static int ota_http_process_body(const uint8_t *data, size_t len, uint8_t *out, size_t cap, size_t *produced)
{
    size_t direct;
    size_t remain;

    if (data == NULL || out == NULL || produced == NULL) {
        return -1;
    }
    direct = cap - *produced;
    if (direct > len) {
        direct = len;
    }
    if (direct > 0U) {
        memcpy(out + *produced, data, direct);
        *produced += direct;
        s_http.delivered_offset += (uint32_t)direct;
    }

    remain = len - direct;
    if (remain > 0U) {
        if (s_http.spill_len != 0U || remain > sizeof(s_http.spill)) {
            return -1;
        }
        memcpy(s_http.spill, data + direct, remain);
        s_http.spill_len = remain;
        s_http.spill_pos = 0U;
    }

    ota_http_note_body_bytes(len);
    return 0;
}

static int ota_http_consume_chunk(const uint8_t *chunk, size_t chunk_len, uint8_t *out, size_t cap, size_t *produced)
{
    size_t header_bytes;
    size_t body_len;

    if (chunk == NULL || out == NULL || produced == NULL) {
        return -1;
    }

    if (s_http.headers_done == 0U) {
        if (s_http.header_len + chunk_len > sizeof(s_http.header_buf)) {
            return -1;
        }
        memcpy(s_http.header_buf + s_http.header_len, chunk, chunk_len);
        s_http.header_len += chunk_len;
        if (ota_http_find_header_end(s_http.header_buf, s_http.header_len, &header_bytes) != 0) {
            return 0;
        }
        if (ota_http_parse_headers(s_http.header_buf, header_bytes, s_http.request_offset, &s_http.content_length) != 0) {
            return -1;
        }
        s_http.headers_done = 1U;
        s_http.has_content_length = 1U;
        body_len = s_http.header_len - header_bytes;
        if (body_len > 0U &&
            ota_http_process_body((const uint8_t *)s_http.header_buf + header_bytes, body_len,
                                  out, cap, produced) != 0) {
            return -1;
        }
        s_http.header_len = 0U;
        return 0;
    }

    return ota_http_process_body(chunk, chunk_len, out, cap, produced);
}

static int ota_http_open_session(const char *url, uint32_t offset)
{
    ota_http_url_t parsed;
    char request[384];

    if (ota_http_parse_url(url, &parsed) != 0) {
        return -1;
    }
    if (ota_http_pause_network() != 0) {
        return -1;
    }
    ota_http_close_session();

    if (net_4g_modem_tcp_connect(parsed.host, parsed.port) != 0) {
        return -1;
    }

    if (offset > 0U) {
        (void)snprintf(request, sizeof(request),
                       "GET %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "User-Agent: ctrl-ota/1\r\n"
                       "Connection: close\r\n"
                       "Range: bytes=%lu-\r\n"
                       "\r\n",
                       parsed.path, parsed.host, (unsigned long)offset);
    } else {
        (void)snprintf(request, sizeof(request),
                       "GET %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "User-Agent: ctrl-ota/1\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       parsed.path, parsed.host);
    }

    if (net_4g_modem_tcp_send((const uint8_t *)request, strlen(request)) < 0) {
        net_4g_modem_tcp_close();
        return -1;
    }

    memset(&s_http, 0, sizeof(s_http));
    s_http.active = 1U;
    s_http.paused_network = 1U;
    s_http.endpoint = parsed;
    s_http.request_offset = offset;
    s_http.delivered_offset = offset;
    s_http.received_offset = offset;
    s_http.poll_clock_ms = 0U;
    (void)ota_copy_text(s_http.url, sizeof(s_http.url), url);
    return 0;
}

static int ota_http_read_bytes(uint8_t *out, size_t cap, size_t *out_nread)
{
    uint8_t temp[OTA_HTTP_TEMP_RX_MAX];
    size_t produced = 0U;
    uint32_t waited_ms = 0U;

    if (out == NULL || out_nread == NULL) {
        return -1;
    }
    *out_nread = 0U;

    if (ota_http_copy_spill(out, cap, &produced) != 0) {
        return -1;
    }
    if (produced >= cap) {
        *out_nread = produced;
        return 0;
    }

    while (produced < cap) {
        size_t n = net_4g_modem_tcp_rx_pop(temp, sizeof(temp));
        if (n > 0U) {
            waited_ms = 0U;
            if (ota_http_consume_chunk(temp, n, out, cap, &produced) != 0) {
                return -1;
            }
            if (produced > 0U && (produced >= cap || s_http.spill_len > 0U)) {
                break;
            }
            continue;
        }

        if (s_http.eof != 0U) {
            break;
        }
        if (net_4g_modem_tcp_is_connected() == 0) {
            if (s_http.has_content_length != 0U &&
                s_http.received_offset >= s_http.request_offset + s_http.content_length) {
                s_http.eof = 1U;
                break;
            }
            if (produced > 0U) {
                break;
            }
            return -1;
        }

        if (waited_ms >= OTA_HTTP_BODY_WAIT_MS) {
            if (produced > 0U) {
                break;
            }
            return -1;
        }
        bsp_system_delay_ms(OTA_HTTP_POLL_STEP_MS);
        waited_ms += OTA_HTTP_POLL_STEP_MS;
        s_http.poll_clock_ms += OTA_HTTP_POLL_STEP_MS;
        net_4g_modem_poll(s_http.poll_clock_ms);
    }

    *out_nread = produced;
    return 0;
}

static int download_chunk_http(const char *url, uint32_t offset, uint8_t *out,
                               size_t cap, size_t *out_nread, void *user)
{
    int rc;

    (void)user;
    if (url == NULL || out == NULL || out_nread == NULL || cap == 0U) {
        return -1;
    }
    *out_nread = 0U;

    if (s_http.active == 0U ||
        ota_same_text(s_http.url, url) == 0 ||
        s_http.delivered_offset != offset) {
        if (ota_http_open_session(url, offset) != 0) {
            ota_http_close_session();
            ota_http_resume_network();
            return -1;
        }
    }

    rc = ota_http_read_bytes(out, cap, out_nread);
    if (rc != 0) {
        ota_http_close_session();
        ota_http_resume_network();
        return -1;
    }
    if (*out_nread == 0U && s_http.eof != 0U) {
        ota_http_close_session();
    }
    return 0;
}

static int erase_upgrade_region(void *user)
{
    uint32_t addr;

    (void)user;
    ota_http_close_session();
    for (addr = FLASH_STAGING_SLOT_ADDR;
         addr < FLASH_STAGING_SLOT_END;
         addr += STM32F103_FLASH_PAGE_SIZE_BYTES) {
        if (bsp_flash_erase_sector(addr) != 0) {
            ota_http_resume_network();
            return -1;
        }
    }
    return 0;
}

static int write_upgrade_region(uint32_t offset, const uint8_t *data, size_t len, void *user)
{
    uint32_t addr;

    (void)user;
    if ((data == NULL && len != 0U) || offset > FLASH_STAGING_SLOT_SIZE_BYTES) {
        return -1;
    }
    if ((uint64_t)offset + (uint64_t)len > (uint64_t)FLASH_STAGING_SLOT_SIZE_BYTES) {
        return -1;
    }
    addr = FLASH_STAGING_SLOT_ADDR + offset;
    return bsp_flash_write(addr, data, len);
}

static void sha256_transform(ota_sha256_ctx_t *ctx, const uint8_t block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    uint32_t i;

    for (i = 0U; i < 16U; i++) {
        w[i] = ((uint32_t)block[i * 4U] << 24) |
               ((uint32_t)block[i * 4U + 1U] << 16) |
               ((uint32_t)block[i * 4U + 2U] << 8) |
               (uint32_t)block[i * 4U + 3U];
    }
    for (i = 16U; i < 64U; i++) {
        uint32_t s0 = ((w[i - 15U] >> 7) | (w[i - 15U] << 25)) ^
                      ((w[i - 15U] >> 18) | (w[i - 15U] << 14)) ^
                      (w[i - 15U] >> 3);
        uint32_t s1 = ((w[i - 2U] >> 17) | (w[i - 2U] << 15)) ^
                      ((w[i - 2U] >> 19) | (w[i - 2U] << 13)) ^
                      (w[i - 2U] >> 10);
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
        uint32_t s1 = ((e >> 6) | (e << 26)) ^
                      ((e >> 11) | (e << 21)) ^
                      ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t s0 = ((a >> 2) | (a << 30)) ^
                      ((a >> 13) | (a << 19)) ^
                      ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + s1 + ch + k[i] + w[i];
        t2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
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

static int sha256_init_ctx(void **out_ctx, void *user)
{
    ota_sha256_ctx_t *ctx;

    (void)user;
    if (out_ctx == NULL) {
        return -1;
    }
    static ota_sha256_ctx_t s_ctx;
    ctx = &s_ctx;
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    *out_ctx = ctx;
    return 0;
}

static int sha256_update_ctx(void *ctx_ptr, const uint8_t *data, size_t len, void *user)
{
    ota_sha256_ctx_t *ctx = (ota_sha256_ctx_t *)ctx_ptr;
    size_t i;

    (void)user;
    if (ctx == NULL || (data == NULL && len != 0U)) {
        return -1;
    }
    for (i = 0U; i < len; i++) {
        ctx->block[ctx->block_len++] = data[i];
        if (ctx->block_len == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->bit_count += 512ULL;
            ctx->block_len = 0U;
        }
    }
    return 0;
}

static int sha256_final_ctx(void *ctx_ptr, uint8_t out_digest[32], void *user)
{
    ota_sha256_ctx_t *ctx = (ota_sha256_ctx_t *)ctx_ptr;
    uint64_t total_bits;
    size_t i;

    (void)user;
    if (ctx == NULL || out_digest == NULL) {
        return -1;
    }

    total_bits = ctx->bit_count + ((uint64_t)ctx->block_len * 8ULL);
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
        ctx->block[63U - i] = (uint8_t)(total_bits >> (i * 8U));
    }
    sha256_transform(ctx, ctx->block);

    for (i = 0U; i < 8U; i++) {
        out_digest[i * 4U] = (uint8_t)(ctx->state[i] >> 24);
        out_digest[i * 4U + 1U] = (uint8_t)(ctx->state[i] >> 16);
        out_digest[i * 4U + 2U] = (uint8_t)(ctx->state[i] >> 8);
        out_digest[i * 4U + 3U] = (uint8_t)(ctx->state[i]);
    }
    return 0;
}

static void sha256_free_ctx(void *ctx_ptr, void *user)
{
    (void)user;
    if (ctx_ptr != NULL) {
        memset(ctx_ptr, 0, sizeof(ota_sha256_ctx_t));
    }
}

static void reboot_to_new_image(void *user)
{
    ota_prepare_payload_t manifest;

    (void)user;
    ota_http_close_session();
    if (storage_upgrade_load_manifest(&manifest) != 0) {
        bsp_debug_log("[OTA] reboot rejected: manifest missing\r\n");
        ota_http_resume_network();
        return;
    }
    if (boot_control_schedule_upgrade(manifest.package_size) != 0) {
        bsp_debug_log("[OTA] reboot rejected: boot control write failed\r\n");
        ota_http_resume_network();
        return;
    }

    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
    *(volatile uint32_t *)SCB_AIRCR_ADDR = SCB_AIRCR_SYSRESET;
    __asm volatile("dsb" ::: "memory");
    for (;;) {
    }
}

static const ota_port_t s_port = {
    .tcp_session_stable  = read_tcp_ok,
    .get_battery_soc     = read_battery_soc,
    .get_signal_csq      = read_signal_csq,
    .http_download_chunk = download_chunk_http,
    .sha256_init         = sha256_init_ctx,
    .sha256_update       = sha256_update_ctx,
    .sha256_final        = sha256_final_ctx,
    .sha256_free         = sha256_free_ctx,
    .flash_erase_upgrade_region = erase_upgrade_region,
    .flash_write_upgrade_region = write_upgrade_region,
    .reboot_to_new_image = reboot_to_new_image,
    .storage_free_bytes  = read_storage_free_bytes,
    .user                = NULL,
};

const ota_port_t *ota_port_board(void)
{
    return &s_port;
}
