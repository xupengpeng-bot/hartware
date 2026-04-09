/**
 * 北向业务链路（单线程轮询）：
 * 1) net_4g_modem：上电/离线恢复 → AT 在线 → PDP(AT+QIACT) → TCP(AT+QIOPEN)。
 * 2) net_socket_client：把模组 RX 解析为帧；send 走 QISEND。
 * 3) TCP 通后 net_connectivity_try_register：proto_register JSON → proto_envelope 发往平台。
 * 4) proto_dispatch_handle_inbound：收平台 JSON，ACK/NACK 经 envelope 回写。
 * 5) app_scheduler：link_ping / vitals / snapshot 周期调用 net_connectivity_send_json。
 *
 * 省流：传输层可开 TCP keepalive；应用层用 hb_kind=ping 保活，vitals 低频或变化触发。
 */
#include "net_connectivity.h"
#include "net_socket_client.h"
#include "net_4g_modem.h"
#include "net_platform_config.h"
#include "proto_dispatch.h"
#include "proto_envelope.h"
#include "proto_register.h"
#include "common_status.h"

#include <stdbool.h>
#include <string.h>

static net_socket_client_t s_sock;
static uint32_t            s_last_connect_attempt_ms;
static uint8_t             s_register_pending;

static char    s_nc_json[4096];
static char    s_nc_reply[4096];
static char    s_nc_register[1956];
static uint8_t s_nc_wire[8192];

#define NET_RECONNECT_BACKOFF_MS 5000U

static void net_connectivity_mark_disconnected(void)
{
    if (s_sock.connected != 0) {
        net_socket_client_disconnect(&s_sock);
    }
    common_status_set_tcp_connected(false);
    common_status_set_registered(false);
    s_register_pending = 1U;
}

static void net_connectivity_try_register(void)
{
    if (s_register_pending == 0U || s_sock.connected == 0) {
        return;
    }
    {
        int n = proto_register_build(s_nc_register, sizeof(s_nc_register));
        if (n > 0 && net_connectivity_send_json(s_nc_register, (size_t)n) >= 0) {
            common_status_set_registered(true);
            s_register_pending = 0U;
        }
    }
}

void net_connectivity_init(void)
{
    net_platform_config_init();
    net_socket_client_init(&s_sock);
    net_4g_modem_init();
    s_last_connect_attempt_ms = 0U;
    s_register_pending        = 1U;
    common_status_set_online(net_4g_modem_is_online());
    common_status_set_registered(false);

    const net_platform_endpoint_t *ep = net_platform_endpoint_get();
    if (net_4g_modem_is_online() && ep->tcp_host[0] != '\0' && ep->tcp_port != 0U) {
        if (net_socket_client_connect(&s_sock, ep->tcp_host, ep->tcp_port) == 0) {
            net_connectivity_try_register();
        }
    }
    common_status_set_tcp_connected(s_sock.connected != 0);
}

void net_connectivity_poll(uint32_t monotonic_ms)
{
    net_4g_modem_poll(monotonic_ms);
    common_status_set_online(net_4g_modem_is_online());

    if (!net_4g_modem_is_online()) {
        net_connectivity_mark_disconnected();
        return;
    }

    if (s_sock.connected != 0 && net_4g_modem_tcp_is_connected() == 0) {
        net_socket_client_disconnect(&s_sock);
        common_status_set_tcp_connected(false);
        common_status_set_registered(false);
        s_register_pending = 1U;
    }

    {
        const net_platform_endpoint_t *ep = net_platform_endpoint_get();
        if (s_sock.connected == 0 && ep->tcp_host[0] != '\0' && ep->tcp_port != 0U) {
            if (s_last_connect_attempt_ms == 0U ||
                (uint32_t)(monotonic_ms - s_last_connect_attempt_ms) >= NET_RECONNECT_BACKOFF_MS) {
                s_last_connect_attempt_ms = monotonic_ms;
                if (net_socket_client_connect(&s_sock, ep->tcp_host, ep->tcp_port) == 0) {
                    common_status_set_tcp_connected(true);
                    common_status_set_registered(false);
                    s_register_pending = 1U;
                }
            }
        }
    }

    common_status_set_tcp_connected(s_sock.connected != 0);
    if (s_sock.connected == 0) {
        return;
    }

    net_connectivity_try_register();

    while (1) {
        size_t jlen = 0U;
        if (net_socket_client_poll(&s_sock, monotonic_ms, s_nc_json, sizeof(s_nc_json), &jlen) != 1) {
            break;
        }
        {
            size_t rlen = 0U;
            (void)proto_dispatch_handle_inbound(s_nc_json, jlen, s_nc_reply, sizeof(s_nc_reply), &rlen);
            if (rlen > 0U) {
                int w = proto_envelope_encode(s_nc_reply, rlen, s_nc_wire, sizeof(s_nc_wire));
                if (w > 0) {
                    (void)net_socket_client_send(&s_sock, s_nc_wire, (size_t)w);
                }
            }
        }
        if (s_sock.connected == 0) {
            break;
        }
    }

    if (s_sock.connected == 0) {
        common_status_set_tcp_connected(false);
        common_status_set_registered(false);
        s_register_pending = 1U;
    }
}

int net_connectivity_send_json(const char *json_body, size_t json_len)
{
    if (!json_body || json_len == 0U) {
        return -1;
    }
    int w = proto_envelope_encode(json_body, json_len, s_nc_wire, sizeof(s_nc_wire));
    if (w < 0) {
        return -2;
    }
    if (s_sock.connected == 0) {
        return -3;
    }
    return net_socket_client_send(&s_sock, s_nc_wire, (size_t)w);
}
