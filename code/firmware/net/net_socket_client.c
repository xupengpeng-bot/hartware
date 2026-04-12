#include "net_socket_client.h"
#include "proto_envelope.h"
#include "bsp_uart.h"

#include <stdio.h>
#include <string.h>

#if defined(BOARD_STM32F103)
#include "net_4g_modem.h"
#endif

void net_socket_client_init(net_socket_client_t *c)
{
    if (!c) {
        return;
    }
    memset(c, 0, sizeof(*c));
}

int net_socket_client_connect(net_socket_client_t *c, const char *host, uint16_t port)
{
    if (!c) {
        return -1;
    }
#if defined(BOARD_STM32F103)
    if (host != NULL) {
        (void)strncpy(c->host, host, sizeof(c->host) - 1U);
        c->host[sizeof(c->host) - 1U] = '\0';
    } else {
        c->host[0] = '\0';
    }
    c->port     = port;
    c->socket_id = 0U;
    if (net_4g_modem_tcp_connect(host, port) != 0) {
        c->connected = 0;
        return -1;
    }
    c->connected = 1;
    return 0;
#else
    (void)host;
    (void)port;
    c->connected = 1;
    return 0;
#endif
}

void net_socket_client_disconnect(net_socket_client_t *c)
{
    if (!c) {
        return;
    }
#if defined(BOARD_STM32F103)
    net_4g_modem_tcp_close();
#endif
    c->connected = 0;
    c->rx_len    = 0U;
}

int net_socket_client_send(net_socket_client_t *c, const uint8_t *data, size_t len)
{
    if (!c || !data || len == 0U) {
        return 0;
    }
#if defined(BOARD_STM32F103)
    if (c->connected == 0 || net_4g_modem_tcp_is_connected() == 0) {
        return -1;
    }
    {
        size_t         total = 0U;
        const uint8_t *p     = data;
        size_t         remain = len;
        while (remain > 0U) {
            size_t chunk = remain > 1460U ? 1460U : remain;
            int    w     = net_4g_modem_tcp_send(p, chunk);
            if (w < 0) {
                return -1;
            }
            total += (size_t)w;
            p += chunk;
            remain -= chunk;
        }
        return (int)total;
    }
#else
    (void)c;
    return 0;
#endif
}

static void shift_left(net_socket_client_t *c, size_t n)
{
    if (n >= c->rx_len) {
        c->rx_len = 0U;
        return;
    }
    memmove(c->rx, c->rx + n, c->rx_len - n);
    c->rx_len -= n;
}

static void net_socket_log_rx_state(const char *event,
                                    size_t chunk_len,
                                    size_t buffered_len,
                                    uint32_t prefix_len,
                                    size_t body_have,
                                    size_t body_missing)
{
    char line[176];

    (void)snprintf(line, sizeof(line),
                   "[PROTO] RX %s chunk=%lu buffered=%lu prefix=%lu body_have=%lu body_missing=%lu\r\n",
                   event != NULL ? event : "state",
                   (unsigned long)chunk_len,
                   (unsigned long)buffered_len,
                   (unsigned long)prefix_len,
                   (unsigned long)body_have,
                   (unsigned long)body_missing);
    bsp_debug_log(line);
}

int net_socket_client_feed(net_socket_client_t *c, const uint8_t *chunk, size_t chunk_len,
                           char *out_json, size_t out_cap, size_t *out_json_len)
{
    uint32_t plen;
    size_t frame;

    if (out_json_len) {
        *out_json_len = 0U;
    }
    if (!c || !out_json || out_cap == 0U) {
        return 0;
    }
    if (chunk != NULL && chunk_len > 0U) {
        if (c->rx_len + chunk_len > sizeof(c->rx)) {
            char line[160];
            (void)snprintf(line, sizeof(line),
                           "[PROTO] RX buffer overflow buffered=%lu incoming=%lu cap=%lu, drop buffered data\r\n",
                           (unsigned long)c->rx_len,
                           (unsigned long)chunk_len,
                           (unsigned long)sizeof(c->rx));
            bsp_debug_log(line);
            c->rx_len = 0U;
            return 0;
        }
        memcpy(c->rx + c->rx_len, chunk, chunk_len);
        c->rx_len += chunk_len;
    }
    if (c->rx_len < PROTO_LENGTH_PREFIX_BYTES) {
        if (chunk != NULL && chunk_len > 0U) {
            net_socket_log_rx_state("wait_prefix",
                                    chunk_len,
                                    c->rx_len,
                                    0U,
                                    0U,
                                    PROTO_LENGTH_PREFIX_BYTES - c->rx_len);
        }
        return 0;
    }
    plen = ((uint32_t)c->rx[0] << 24) | ((uint32_t)c->rx[1] << 16) | ((uint32_t)c->rx[2] << 8) |
           (uint32_t)c->rx[3];
    frame = PROTO_LENGTH_PREFIX_BYTES + (size_t)plen;
    if (plen > sizeof(c->rx) || frame > sizeof(c->rx)) {
        char line[160];
        (void)snprintf(line, sizeof(line),
                       "[PROTO] RX invalid prefix=%lu buffered=%lu cap=%lu, shift=1 for resync\r\n",
                       (unsigned long)plen,
                       (unsigned long)c->rx_len,
                       (unsigned long)sizeof(c->rx));
        bsp_debug_log(line);
        shift_left(c, 1U);
        return 0;
    }
    if (c->rx_len < frame) {
        if (chunk != NULL && chunk_len > 0U) {
            size_t body_have = c->rx_len > PROTO_LENGTH_PREFIX_BYTES ?
                               (c->rx_len - PROTO_LENGTH_PREFIX_BYTES) : 0U;
            size_t body_missing = (size_t)plen > body_have ? ((size_t)plen - body_have) : 0U;
            net_socket_log_rx_state("partial_frame",
                                    chunk_len,
                                    c->rx_len,
                                    plen,
                                    body_have,
                                    body_missing);
        }
        return 0;
    }
    if (plen + 1U > out_cap) {
        char line[160];
        (void)snprintf(line, sizeof(line),
                       "[PROTO] RX frame drop prefix=%lu out_cap=%lu buffered=%lu\r\n",
                       (unsigned long)plen,
                       (unsigned long)out_cap,
                       (unsigned long)c->rx_len);
        bsp_debug_log(line);
        shift_left(c, frame);
        return 0;
    }
    memcpy(out_json, c->rx + PROTO_LENGTH_PREFIX_BYTES, plen);
    out_json[plen] = '\0';
    if (out_json_len) {
        *out_json_len = plen;
    }
    shift_left(c, frame);
    net_socket_log_rx_state("frame_ready",
                            chunk_len,
                            c->rx_len,
                            plen,
                            (size_t)plen,
                            0U);
    return 1;
}

int net_socket_client_poll(net_socket_client_t *c, uint32_t monotonic_ms, char *out_json, size_t out_cap,
                           size_t *out_json_len)
{
    (void)monotonic_ms;
#if defined(BOARD_STM32F103)
    uint8_t chunk[256];
    size_t  n;
    while ((n = net_4g_modem_tcp_rx_pop(chunk, sizeof(chunk))) > 0U) {
        int feed_rc = net_socket_client_feed(c, chunk, n, out_json, out_cap, out_json_len);
        if (feed_rc == 1) {
            return 1;
        }
    }
#endif
    return net_socket_client_feed(c, NULL, 0U, out_json, out_cap, out_json_len);
}
