#ifndef NET_SOCKET_CLIENT_H
#define NET_SOCKET_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#define NET_SOCKET_CLIENT_HOST_MAX 96U

typedef struct {
    uint8_t  rx[8192];
    size_t   rx_len;
    int      connected;
    uint8_t  socket_id;
    char     host[NET_SOCKET_CLIENT_HOST_MAX];
    uint16_t port;
} net_socket_client_t;

void net_socket_client_init(net_socket_client_t *c);
int  net_socket_client_connect(net_socket_client_t *c, const char *host, uint16_t port);
void net_socket_client_disconnect(net_socket_client_t *c);
int  net_socket_client_send(net_socket_client_t *c, const uint8_t *data, size_t len);
/** Push bytes from driver ISR / poll; returns 1 if one full JSON frame was extracted into out_json. */
int  net_socket_client_feed(net_socket_client_t *c, const uint8_t *chunk, size_t chunk_len,
                            char *out_json, size_t out_cap, size_t *out_json_len);
/** Drain modem RX into feed then try extract one frame (4G builds only). */
int  net_socket_client_poll(net_socket_client_t *c, uint32_t monotonic_ms,
                            char *out_json, size_t out_cap, size_t *out_json_len);

#endif /* NET_SOCKET_CLIENT_H */
