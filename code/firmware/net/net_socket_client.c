#include "net_socket_client.h"
#include "proto_envelope.h"
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

int net_socket_client_feed(net_socket_client_t *c, const uint8_t *chunk, size_t chunk_len,
                           char *out_json, size_t out_cap, size_t *out_json_len)
{
    if (out_json_len) {
        *out_json_len = 0U;
    }
    if (!c || !out_json || out_cap == 0U) {
        return 0;
    }
    if (chunk != NULL && chunk_len > 0U) {
        if (c->rx_len + chunk_len > sizeof(c->rx)) {
            c->rx_len = 0U;
            return 0;
        }
        memcpy(c->rx + c->rx_len, chunk, chunk_len);
        c->rx_len += chunk_len;
    }
    if (c->rx_len < PROTO_LENGTH_PREFIX_BYTES) {
        return 0;
    }
    uint32_t plen = ((uint32_t)c->rx[0] << 24) | ((uint32_t)c->rx[1] << 16) | ((uint32_t)c->rx[2] << 8) |
                    (uint32_t)c->rx[3];
    size_t frame = PROTO_LENGTH_PREFIX_BYTES + (size_t)plen;
    if (plen > sizeof(c->rx) || frame > sizeof(c->rx)) {
        shift_left(c, 1U);
        return 0;
    }
    if (c->rx_len < frame) {
        return 0;
    }
    if (plen + 1U > out_cap) {
        shift_left(c, frame);
        return 0;
    }
    memcpy(out_json, c->rx + PROTO_LENGTH_PREFIX_BYTES, plen);
    out_json[plen] = '\0';
    if (out_json_len) {
        *out_json_len = plen;
    }
    shift_left(c, frame);
    return 1;
}

int net_socket_client_poll(net_socket_client_t *c, uint32_t monotonic_ms, char *out_json, size_t out_cap,
                           size_t *out_json_len)
{
    (void)monotonic_ms;
#if defined(BOARD_STM32F103)
    uint8_t chunk[512];
    size_t  n;
    while ((n = net_4g_modem_tcp_rx_pop(chunk, sizeof(chunk))) > 0U) {
        (void)net_socket_client_feed(c, chunk, n, out_json, out_cap, NULL);
    }
#endif
    return net_socket_client_feed(c, NULL, 0U, out_json, out_cap, out_json_len);
}
