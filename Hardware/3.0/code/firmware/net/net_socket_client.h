#ifndef NET_SOCKET_CLIENT_H
#define NET_SOCKET_CLIENT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t  rx[8192];
    size_t   rx_len;
    int      connected;
} net_socket_client_t;

void net_socket_client_init(net_socket_client_t *c);
int  net_socket_client_connect(net_socket_client_t *c, const char *host, uint16_t port);
void net_socket_client_disconnect(net_socket_client_t *c);
int  net_socket_client_send(net_socket_client_t *c, const uint8_t *data, size_t len);
/** Push bytes from driver ISR / poll; returns 1 if one full JSON frame was extracted into out_json. */
int  net_socket_client_feed(net_socket_client_t *c, const uint8_t *chunk, size_t chunk_len,
                          char *out_json, size_t out_cap, size_t *out_json_len);

#endif /* NET_SOCKET_CLIENT_H */
