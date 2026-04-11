#ifndef NET_4G_MODEM_H
#define NET_4G_MODEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void net_4g_modem_init(void);
bool net_4g_modem_is_online(void);
uint32_t net_4g_modem_online_age_ms(uint32_t monotonic_ms);
void net_4g_modem_poll(uint32_t monotonic_ms);

/** Quectel AT+QI*：激活 PDP 并连接 TCP（见 net_4g_modem.c）。成功后可 send / poll 收包。 */
int     net_4g_modem_tcp_connect(const char *host, uint16_t port);
void    net_4g_modem_tcp_close(void);
int     net_4g_modem_tcp_send(const uint8_t *data, size_t len);
int     net_4g_modem_tcp_is_connected(void);
size_t  net_4g_modem_tcp_rx_pop(uint8_t *buf, size_t cap);

#endif /* NET_4G_MODEM_H */
