#ifndef NET_4G_MODEM_H
#define NET_4G_MODEM_H

#include <stdbool.h>

void net_4g_modem_init(void);
bool net_4g_modem_is_online(void);
void net_4g_modem_poll(void);

#endif /* NET_4G_MODEM_H */
