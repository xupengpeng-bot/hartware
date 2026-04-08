#ifndef NET_CONNECTIVITY_H
#define NET_CONNECTIVITY_H

#include <stddef.h>
#include <stdint.h>

void net_connectivity_init(void);
void net_connectivity_poll(uint32_t monotonic_ms);

/** Send one length-prefixed tcp-json frame (caller passes raw JSON body only). */
int net_connectivity_send_json(const char *json_body, size_t json_len);

#endif /* NET_CONNECTIVITY_H */
