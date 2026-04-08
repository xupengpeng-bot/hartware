/**
 * 省流与保活建议（与 app_scheduler 分层心跳配合）：
 * - 传输层：在模组/BSP 开启 TCP keepalive（idle/interval/probes），减少仅靠应用 ping 的包量。
 * - 应用层：轻量 HEARTBEAT（hb_kind=ping）维持业务在线判定；体征用 hb_kind=vitals 低频或变化触发。
 */
#include "net_connectivity.h"
#include "net_socket_client.h"
#include "net_4g_modem.h"
#include "net_platform_config.h"
#include "proto_dispatch.h"
#include "proto_envelope.h"
#include "common_status.h"

#include <string.h>

static net_socket_client_t s_sock;

static char   s_nc_json[4096];
static char   s_nc_reply[4096];
static uint8_t s_nc_wire[8192];

void net_connectivity_init(void)
{
    net_platform_config_init();
    net_socket_client_init(&s_sock);
    net_4g_modem_init();
    common_status_set_online(net_4g_modem_is_online());

    const net_platform_endpoint_t *ep = net_platform_endpoint_get();
    if (net_4g_modem_is_online() && ep->tcp_host[0] != '\0' && ep->tcp_port != 0U) {
        (void)net_socket_client_connect(&s_sock, ep->tcp_host, ep->tcp_port);
    }
    common_status_set_tcp_connected(s_sock.connected != 0);
}

void net_connectivity_poll(uint32_t monotonic_ms)
{
    (void)monotonic_ms;
    net_4g_modem_poll();
    common_status_set_online(net_4g_modem_is_online());

    if (s_sock.connected != 0 && net_4g_modem_tcp_is_connected() == 0) {
        net_socket_client_disconnect(&s_sock);
    }
    common_status_set_tcp_connected(s_sock.connected != 0);

    {
        uint8_t chunk[512];
        size_t  n;
        while ((n = net_4g_modem_tcp_rx_pop(chunk, sizeof(chunk))) > 0U) {
            (void)net_socket_client_feed(&s_sock, chunk, n, s_nc_json, sizeof(s_nc_json), NULL);
        }
    }

    for (;;) {
        size_t jlen = 0U;
        if (net_socket_client_feed(&s_sock, NULL, 0U, s_nc_json, sizeof(s_nc_json), &jlen) != 1) {
            break;
        }
        size_t rlen = 0U;
        (void)proto_dispatch_handle_inbound(s_nc_json, jlen, s_nc_reply, sizeof(s_nc_reply), &rlen);
        if (rlen > 0U) {
            int w = proto_envelope_encode(s_nc_reply, rlen, s_nc_wire, sizeof(s_nc_wire));
            if (w > 0) {
                (void)net_socket_client_send(&s_sock, s_nc_wire, (size_t)w);
            }
        }
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
    return net_socket_client_send(&s_sock, s_nc_wire, (size_t)w);
}
