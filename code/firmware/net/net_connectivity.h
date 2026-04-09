#ifndef NET_CONNECTIVITY_H
#define NET_CONNECTIVITY_H

#include <stddef.h>
#include <stdint.h>

void net_connectivity_init(void);
void net_connectivity_poll(uint32_t monotonic_ms);

/** Send one length-prefixed tcp-json frame (caller passes raw JSON body only). */
int net_connectivity_send_json(const char *json_body, size_t json_len);

/** 断开 TCP 并置为待重连/待注册（如 SYNC_CONFIG 切换平台地址后调用）。 */
void net_connectivity_force_reconnect(void);

void net_connectivity_on_register_ack(void);
void net_connectivity_on_register_nack(void);

#endif /* NET_CONNECTIVITY_H */
