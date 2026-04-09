/**
 * 北向业务链路（单线程轮询）：
 * 1) net_4g_modem：上电/离线恢复 → AT 在线 → PDP(AT+QIACT) → TCP(AT+QIOPEN)。
 * 2) net_socket_client：把模组 RX 解析为帧；send 走 QISEND。
 * 3) TCP 通后 net_connectivity_try_register：proto_register JSON → proto_envelope 发往平台。
 * 4) proto_dispatch_handle_inbound：收平台 JSON；REGISTER_ACK/NACK 更新注册态；其它 ACK/NACK 经 envelope 回写。
 * 5) app_scheduler：link_ping / vitals / snapshot 周期调用 net_connectivity_send_json。
 *    SYNC_CONFIG 可带 platform_tcp_host/port，提交后重载端点并 force_reconnect。
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
#include "proto_codec_json.h"
#include "common_status.h"
#include "bsp_uart.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static net_socket_client_t s_sock;
static uint32_t            s_last_connect_attempt_ms;
static uint32_t            s_last_register_attempt_ms;
static uint8_t             s_register_pending;
static uint8_t             s_register_inflight;

static char    s_nc_json[4096];
static char    s_nc_reply[4096];
static char    s_nc_register[1956];
static uint8_t s_nc_wire[8192];

#define NET_RECONNECT_BACKOFF_MS 5000U
#define NET_REGISTER_RETRY_MS    10000U

static int net_json_validate_complete_object(const char *json, size_t len)
{
    size_t start = 0U;
    size_t end = len;
    size_t i;
    int brace_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int escape = 0;

    if (json == NULL || len == 0U) {
        return -1;
    }

    while (start < len && isspace((unsigned char)json[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char)json[end - 1U])) {
        end--;
    }
    if (start >= end || json[start] != '{' || json[end - 1U] != '}') {
        return -2;
    }

    for (i = start; i < end; i++) {
        unsigned char ch = (unsigned char)json[i];
        if (in_string) {
            if (escape) {
                escape = 0;
                continue;
            }
            if (ch == '\\') {
                escape = 1;
                continue;
            }
            if (ch == '"') {
                in_string = 0;
            }
            continue;
        }

        if (ch == '"') {
            in_string = 1;
        } else if (ch == '{') {
            brace_depth++;
        } else if (ch == '}') {
            brace_depth--;
            if (brace_depth < 0) {
                return -3;
            }
        } else if (ch == '[') {
            bracket_depth++;
        } else if (ch == ']') {
            bracket_depth--;
            if (bracket_depth < 0) {
                return -4;
            }
        }
    }

    if (in_string || escape) {
        return -5;
    }
    if (brace_depth != 0 || bracket_depth != 0) {
        return -6;
    }
    return 0;
}

static int net_json_require_nonempty_string(const char *json, const char *key)
{
    char tmp[96];
    if (proto_json_get_string(json, key, tmp, sizeof(tmp)) != 0) {
        return -1;
    }
    return tmp[0] == '\0' ? -2 : 0;
}

static int net_json_validate_business_frame(const char *json, size_t len)
{
    uint32_t seq = 0U;

    if (net_json_validate_complete_object(json, len) != 0) {
        return -1;
    }
    if (net_json_require_nonempty_string(json, "protocol") != 0) {
        return -2;
    }
    if (net_json_require_nonempty_string(json, "type") != 0) {
        return -3;
    }
    if (net_json_require_nonempty_string(json, "imei") != 0) {
        return -4;
    }
    if (net_json_require_nonempty_string(json, "msg_id") != 0) {
        return -5;
    }
    if (net_json_require_nonempty_string(json, "ts") != 0) {
        return -6;
    }
    if (proto_json_get_u32(json, "seq", &seq) != 0) {
        return -7;
    }
    if (strstr(json, "\"payload\"") == NULL) {
        return -8;
    }
    return 0;
}

static void net_log_json_short(const char *tag, const char *json, size_t len)
{
    char line[220];
    size_t shown = len > 120U ? 120U : len;
    if (json == NULL) {
        return;
    }
    (void)snprintf(line, sizeof(line), "[NET] %s len=%lu json=%.*s%s\r\n",
                   tag,
                   (unsigned long)len,
                   (int)shown,
                   json,
                   len > shown ? "..." : "");
    bsp_debug_log(line);
}

static void net_log_target(const char *tag, const char *host, uint16_t port)
{
    char buf[128];
    if (host == NULL || host[0] == '\0') {
        bsp_debug_log("[NET] ");
        bsp_debug_log(tag);
        bsp_debug_log(": (no host)\r\n");
        return;
    }
    {
        size_t hl = strlen(host);
        if (hl > 48U) {
            (void)snprintf(buf, sizeof(buf), "[NET] %s %.48s...:%u\r\n", tag, host, (unsigned)port);
        } else {
            (void)snprintf(buf, sizeof(buf), "[NET] %s %s:%u\r\n", tag, host, (unsigned)port);
        }
    }
    bsp_debug_log(buf);
}

static void net_connectivity_mark_disconnected(void)
{
    if (s_sock.connected != 0) {
        net_socket_client_disconnect(&s_sock);
    }
    common_status_set_tcp_connected(false);
    common_status_set_registered(false);
    s_register_pending = 1U;
    s_register_inflight = 0U;
}

void net_connectivity_force_reconnect(void)
{
    net_connectivity_mark_disconnected();
}

void net_connectivity_on_register_ack(void)
{
    common_status_set_registered(true);
    s_register_pending = 0U;
    s_register_inflight = 0U;
    bsp_debug_log("[NET] register: REGISTER_ACK from platform\r\n");
}

void net_connectivity_on_register_nack(void)
{
    common_status_set_registered(false);
    s_register_pending = 1U;
    s_register_inflight = 0U;
    bsp_debug_log("[NET] register: REGISTER_NACK from platform\r\n");
}

static void net_log_inbound_type(const char *json)
{
    char type_buf[32];
    if (json == NULL) {
        return;
    }
    if (proto_json_get_string(json, "type", type_buf, sizeof(type_buf)) == 0) {
        char line[80];
        (void)snprintf(line, sizeof(line), "[NET] rx: %s\r\n", type_buf);
        bsp_debug_log(line);
    } else {
        bsp_debug_log("[NET] rx: (json without type)\r\n");
    }
}

static void net_connectivity_try_register(uint32_t monotonic_ms)
{
    if (s_register_pending == 0U || s_sock.connected == 0) {
        return;
    }
    if (s_register_inflight != 0U &&
        (uint32_t)(monotonic_ms - s_last_register_attempt_ms) < NET_REGISTER_RETRY_MS) {
        return;
    }
    if (s_register_inflight != 0U) {
        bsp_debug_log("[NET] register: ACK timeout, retry send\r\n");
    }
    {
        int n = proto_register_build(s_nc_register, sizeof(s_nc_register));
        if (n <= 0) {
            bsp_debug_log("[NET] register: proto_register_build failed\r\n");
            return;
        }
        if (net_connectivity_send_json(s_nc_register, (size_t)n) >= 0) {
            bsp_debug_log("[NET] register: sent (envelope on wire)\r\n");
            common_status_set_registered(false);
            s_register_inflight = 1U;
            s_last_register_attempt_ms = monotonic_ms;
        } else {
            bsp_debug_log("[NET] register: send failed (TCP down or encode error)\r\n");
            s_register_inflight = 0U;
        }
    }
}

void net_connectivity_init(void)
{
    net_platform_config_init();
    net_socket_client_init(&s_sock);
    net_4g_modem_init();
    s_last_connect_attempt_ms = 0U;
    s_last_register_attempt_ms = 0U;
    s_register_pending        = 1U;
    s_register_inflight       = 0U;
    common_status_set_online(net_4g_modem_is_online());
    common_status_set_registered(false);

    const net_platform_endpoint_t *ep = net_platform_endpoint_get();
    if (ep->tcp_host[0] == '\0' || ep->tcp_port == 0U) {
        bsp_debug_log("[NET] init: no TCP endpoint (set host/port in config)\r\n");
    } else {
        net_log_target("init: endpoint", ep->tcp_host, ep->tcp_port);
    }
    if (net_4g_modem_is_online() && ep->tcp_host[0] != '\0' && ep->tcp_port != 0U) {
        bsp_debug_log("[NET] init: dialing TCP...\r\n");
        if (net_socket_client_connect(&s_sock, ep->tcp_host, ep->tcp_port) == 0) {
            bsp_debug_log("[NET] init: TCP up\r\n");
            net_connectivity_try_register(0U);
        } else {
            bsp_debug_log("[NET] init: TCP connect failed (see [4G] TCP [n/4] lines)\r\n");
        }
    } else if (!net_4g_modem_is_online()) {
        bsp_debug_log("[NET] init: skip TCP, modem not online yet\r\n");
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
        bsp_debug_log("[NET] poll: modem TCP socket closed, reconnect pending\r\n");
        net_socket_client_disconnect(&s_sock);
        common_status_set_tcp_connected(false);
        common_status_set_registered(false);
        s_register_pending = 1U;
        s_register_inflight = 0U;
    }

    {
        const net_platform_endpoint_t *ep = net_platform_endpoint_get();
        if (s_sock.connected == 0 && ep->tcp_host[0] != '\0' && ep->tcp_port != 0U) {
            if (s_last_connect_attempt_ms == 0U ||
                (uint32_t)(monotonic_ms - s_last_connect_attempt_ms) >= NET_RECONNECT_BACKOFF_MS) {
                s_last_connect_attempt_ms = monotonic_ms;
                net_log_target("TCP reconnect", ep->tcp_host, ep->tcp_port);
                if (net_socket_client_connect(&s_sock, ep->tcp_host, ep->tcp_port) == 0) {
                    bsp_debug_log("[NET] TCP reconnect: OK\r\n");
                    common_status_set_tcp_connected(true);
                    common_status_set_registered(false);
                    s_register_pending = 1U;
                    s_register_inflight = 0U;
                } else {
                    bsp_debug_log("[NET] TCP reconnect: failed\r\n");
                }
            }
        }
    }

    common_status_set_tcp_connected(s_sock.connected != 0);
    if (s_sock.connected == 0) {
        return;
    }

    net_connectivity_try_register(monotonic_ms);

    while (1) {
        size_t jlen = 0U;
        if (net_socket_client_poll(&s_sock, monotonic_ms, s_nc_json, sizeof(s_nc_json), &jlen) != 1) {
            break;
        }
        net_log_json_short("rx-json", s_nc_json, jlen);
        net_log_inbound_type(s_nc_json);
        {
            size_t rlen = 0U;
            int dispatch_rc = proto_dispatch_handle_inbound(s_nc_json, jlen, s_nc_reply, sizeof(s_nc_reply), &rlen);
            if (dispatch_rc > 0) {
                bsp_debug_log("[NET] rx: ignored/unsupported message\r\n");
            } else if (dispatch_rc < 0) {
                bsp_debug_log("[NET] rx: dispatch error\r\n");
            }
            if (rlen > 0U) {
                int w = proto_envelope_encode(s_nc_reply, rlen, s_nc_wire, sizeof(s_nc_wire));
                if (w > 0) {
                    if (net_socket_client_send(&s_sock, s_nc_wire, (size_t)w) >= 0) {
                        bsp_debug_log("[NET] tx: command reply sent\r\n");
                    } else {
                        bsp_debug_log("[NET] tx: command reply send failed\r\n");
                    }
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
    {
        int v = net_json_validate_business_frame(json_body, json_len);
        if (v != 0) {
            char line[96];
            (void)snprintf(line, sizeof(line), "[NET] tx-json blocked: invalid business json rc=%d\r\n", v);
            bsp_debug_log(line);
            net_log_json_short("tx-json-invalid", json_body, json_len);
            return -2;
        }
    }
    net_log_json_short("tx-json", json_body, json_len);
    int w = proto_envelope_encode(json_body, json_len, s_nc_wire, sizeof(s_nc_wire));
    if (w < 0) {
        return -3;
    }
    {
        char line[96];
        uint32_t be_len = ((uint32_t)s_nc_wire[0] << 24) | ((uint32_t)s_nc_wire[1] << 16) |
                          ((uint32_t)s_nc_wire[2] << 8) | (uint32_t)s_nc_wire[3];
        (void)snprintf(line, sizeof(line), "[NET] tx-wire total=%d prefix=%lu\r\n", w, (unsigned long)be_len);
        bsp_debug_log(line);
    }
    if (s_sock.connected == 0) {
        return -4;
    }
    return net_socket_client_send(&s_sock, s_nc_wire, (size_t)w);
}
