#ifndef NET_CONNECTIVITY_H
#define NET_CONNECTIVITY_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t conn_age_ms;
    uint32_t idle_ms;
    size_t   last_tx_len;
    char     reason[24];
    char     last_tx_type[32];
} net_disconnect_diag_t;

void net_connectivity_init(void);
void net_connectivity_poll(uint32_t monotonic_ms);

/** Send one length-prefixed tcp-json frame (caller passes raw JSON body only). */
int net_connectivity_send_json(const char *json_body, size_t json_len);

/** 断开 TCP 并置为待重连/待注册（如 SYNC_CONFIG 切换平台地址后调用）。 */
void net_connectivity_force_reconnect(void);
void net_connectivity_pause_for_ota(void);
void net_connectivity_resume_after_ota(void);

void net_connectivity_on_register_ack(void);
void net_connectivity_on_register_nack(void);
void net_connectivity_get_last_disconnect_diag(net_disconnect_diag_t *out);
uint32_t net_connectivity_last_uplink_age_ms(uint32_t monotonic_ms);

#endif /* NET_CONNECTIVITY_H */
