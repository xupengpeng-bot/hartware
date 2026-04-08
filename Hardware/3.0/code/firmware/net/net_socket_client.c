#include "net_socket_client.h"
#include "proto_envelope.h"
#include <string.h>

void net_socket_client_init(net_socket_client_t *c)
{
    if (!c) {
        return;
    }
    memset(c->rx, 0, sizeof(c->rx));
    c->rx_len = 0U;
    c->connected = 0;
}

int net_socket_client_connect(net_socket_client_t *c, const char *host, uint16_t port)
{
    (void)host;
    (void)port;
    if (!c) {
        return -1;
    }
    c->connected = 1;
    return 0;
}

void net_socket_client_disconnect(net_socket_client_t *c)
{
    if (!c) {
        return;
    }
    c->connected = 0;
    c->rx_len = 0U;
}

int net_socket_client_send(net_socket_client_t *c, const uint8_t *data, size_t len)
{
    (void)c;
    (void)data;
    (void)len;
    /* BSP / LWIP: implement send. */
    return 0;
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
