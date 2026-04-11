#include "net_connectivity.h"

#include "common_status.h"
#include "bsp_rtc.h"
#include "net_4g_modem.h"
#include "net_platform_config.h"
#include "net_socket_client.h"
#include "proto_codec_json.h"
#include "proto_dispatch.h"
#include "proto_envelope.h"
#include "proto_register.h"
#include "runtime_state.h"
#include "common_identity.h"
#include "cJSON.h"

#include "bsp_uart.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static net_socket_client_t s_sock;
static uint32_t s_last_connect_attempt_ms;
static uint32_t s_last_register_attempt_ms;
static uint32_t s_next_register_attempt_ms;
static uint32_t s_last_poll_ms;
static uint32_t s_tcp_connected_since_ms;
static uint32_t s_last_tx_ms;
static uint8_t s_register_pending;
static uint8_t s_register_inflight;
static uint8_t s_register_retry_stage;
static size_t   s_last_tx_len;
static uint32_t s_register_retry_prng;
static char     s_last_tx_type[32];
static net_disconnect_diag_t s_last_disconnect_diag;

#define NET_JSON_FRAME_MAX      2048U
#define NET_WIRE_FRAME_MAX      (PROTO_LENGTH_PREFIX_BYTES + NET_JSON_FRAME_MAX)
#define NET_RECONNECT_BACKOFF_MS 15000U
#define NET_REGISTER_IDENTITY_RETRY_MS 10000U
#define NET_MODEM_WARMUP_MS     20000U
#define NET_MAX_INBOUND_FRAMES_PER_POLL 2U

static char s_rx_json[NET_JSON_FRAME_MAX];
static char s_tx_json[NET_JSON_FRAME_MAX];
static uint8_t s_wire[NET_WIRE_FRAME_MAX];

static void net_log_disconnect_context(const char *reason, uint32_t monotonic_ms);

static int net_json_validate_complete_object(const char *json, size_t len)
{
    cJSON *parsed;
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
            } else if (ch == '\\') {
                escape = 1;
            } else if (ch == '"') {
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
    if (in_string || escape || brace_depth != 0 || bracket_depth != 0) {
        return -5;
    }
    parsed = cJSON_ParseWithLength(json + start, end - start);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        if (parsed != NULL) {
            cJSON_Delete(parsed);
        }
        return -6;
    }
    cJSON_Delete(parsed);
    return 0;
}

static int net_json_require_nonempty_string(const char *json, const char *key)
{
    char tmp[48];

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
    if (net_json_require_nonempty_string(json, "t") != 0 ||
        net_json_require_nonempty_string(json, "i") != 0 ||
        net_json_require_nonempty_string(json, "m") != 0) {
        return -2;
    }
    if (proto_json_get_u32(json, "v", &seq) != 0 || seq != 1U) {
        return -3;
    }
    if (proto_json_get_u32(json, "s", &seq) != 0 || seq == 0U) {
        return -4;
    }
    if (strstr(json, "\"p\"") == NULL) {
        return -7;
    }
    return 0;
}

static void net_log_json(const char *prefix, const char *json, size_t len)
{
    char line[256];
    size_t shown = len > 140U ? 140U : len;

    if (json == NULL) {
        return;
    }
    (void)snprintf(line, sizeof(line), "[JSON] %s len=%lu %.*s%s\r\n",
                   prefix,
                   (unsigned long)len,
                   (int)shown,
                   json,
                   len > shown ? "..." : "");
    bsp_debug_log(line);
}

static void net_log_full_json(const char *type, const char *json, size_t len)
{
    char line[224];
    size_t offset = 0U;

    if (type == NULL || json == NULL || len == 0U) {
        return;
    }
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > 160U) {
            chunk = 160U;
        }
        (void)snprintf(line, sizeof(line), "[JSON-FULL] %s part=%lu %.*s\r\n",
                       type,
                       (unsigned long)(offset / 160U),
                       (int)chunk,
                       json + offset);
        bsp_debug_log(line);
        offset += chunk;
    }
}

static void net_log_prefix(uint32_t prefix_len, size_t total_len)
{
    char line[96];

    (void)snprintf(line, sizeof(line), "[PROTO] be32_len=%lu wire_len=%lu\r\n",
                   (unsigned long)prefix_len,
                   (unsigned long)total_len);
    bsp_debug_log(line);
}

static void net_log_json_tail(const char *json, size_t len)
{
    char line[160];
    size_t tail_len;
    const char *tail;

    if (json == NULL || len == 0U) {
        return;
    }
    tail_len = len > 32U ? 32U : len;
    tail = json + (len - tail_len);
    (void)snprintf(line, sizeof(line), "[PROTO] payload_tail32=%.*s\r\n",
                   (int)tail_len, tail);
    bsp_debug_log(line);
}

static void net_mark_disconnected(void)
{
    if (s_sock.connected != 0) {
        net_socket_client_disconnect(&s_sock);
    }
    common_status_set_tcp_connected(false);
    common_status_set_registered(false);
    s_register_pending = 1U;
    s_register_inflight = 0U;
    s_tcp_connected_since_ms = 0U;
}

static void net_handle_disconnect(const char *reason, uint32_t monotonic_ms, uint8_t count_network_lost)
{
    if (reason == NULL) {
        reason = "peer_closed";
    }
    net_log_disconnect_context(reason, monotonic_ms);
    if (count_network_lost != 0U) {
        runtime_state_inc_counter_network_lost();
    }
    net_mark_disconnected();
}

static void net_register_retry_reset(void)
{
    s_register_retry_stage = 0U;
    s_next_register_attempt_ms = 0U;
}

static uint32_t net_register_retry_jitter(uint32_t monotonic_ms)
{
    s_register_retry_prng = s_register_retry_prng * 1664525UL + 1013904223UL + monotonic_ms;
    return s_register_retry_prng % 1001U;
}

static void net_register_retry_schedule(uint32_t monotonic_ms, const char *reason)
{
    static const uint32_t k_backoff_ms[] = { 2000U, 4000U, 8000U, 30000U };
    uint32_t base_ms;
    uint32_t jitter_ms;
    char line[160];

    if (s_register_retry_stage >= (uint8_t)(sizeof(k_backoff_ms) / sizeof(k_backoff_ms[0]))) {
        s_register_retry_stage = (uint8_t)(sizeof(k_backoff_ms) / sizeof(k_backoff_ms[0])) - 1U;
    }
    base_ms = k_backoff_ms[s_register_retry_stage];
    jitter_ms = net_register_retry_jitter(monotonic_ms);
    s_next_register_attempt_ms = monotonic_ms + base_ms + jitter_ms;
    if (s_register_retry_stage + 1U < (uint8_t)(sizeof(k_backoff_ms) / sizeof(k_backoff_ms[0]))) {
        s_register_retry_stage++;
    }
    (void)snprintf(line, sizeof(line),
                   "[PROTO] REGISTER retry scheduled reason=%s base=%lu ms jitter=%lu ms next_in=%lu ms\r\n",
                   reason != NULL ? reason : "unknown",
                   (unsigned long)base_ms,
                   (unsigned long)jitter_ms,
                   (unsigned long)(base_ms + jitter_ms));
    bsp_debug_log(line);
}

static void net_note_tx_context(const char *json_body, size_t json_len, uint32_t monotonic_ms)
{
    char type_buf[32];

    if (json_body == NULL || json_len == 0U) {
        return;
    }
    memset(type_buf, 0, sizeof(type_buf));
    if (proto_json_get_string(json_body, "type", type_buf, sizeof(type_buf)) != 0 || type_buf[0] == '\0') {
        (void)strncpy(type_buf, "UNKNOWN", sizeof(type_buf) - 1U);
    }
    (void)strncpy(s_last_tx_type, type_buf, sizeof(s_last_tx_type) - 1U);
    s_last_tx_type[sizeof(s_last_tx_type) - 1U] = '\0';
    s_last_tx_ms = monotonic_ms;
    s_last_tx_len = json_len;
}

static void net_log_disconnect_context(const char *reason, uint32_t monotonic_ms)
{
    char line[256];
    const runtime_state_t *rs = runtime_state_get();
    uint32_t conn_age_ms = 0U;
    uint32_t idle_ms = 0U;

    if (reason == NULL || rs == NULL) {
        return;
    }
    if (s_tcp_connected_since_ms != 0U) {
        conn_age_ms = (uint32_t)(monotonic_ms - s_tcp_connected_since_ms);
    }
    if (s_last_tx_ms != 0U) {
        idle_ms = (uint32_t)(monotonic_ms - s_last_tx_ms);
    }
    memset(&s_last_disconnect_diag, 0, sizeof(s_last_disconnect_diag));
    s_last_disconnect_diag.conn_age_ms = conn_age_ms;
    s_last_disconnect_diag.idle_ms = idle_ms;
    s_last_disconnect_diag.last_tx_len = s_last_tx_len;
    (void)strncpy(s_last_disconnect_diag.reason, reason, sizeof(s_last_disconnect_diag.reason) - 1U);
    (void)snprintf(s_last_disconnect_diag.last_tx_type,
                   sizeof(s_last_disconnect_diag.last_tx_type),
                   "%s",
                   s_last_tx_type[0] != '\0' ? s_last_tx_type : "NONE");
    (void)snprintf(line, sizeof(line),
                   "[PROTO] disconnect ctx reason=%s conn_age=%lu ms last_tx=%s/%luB %lu ms ago workflow=%s run=%s ready=%u\r\n",
                   reason,
                   (unsigned long)conn_age_ms,
                   s_last_tx_type[0] != '\0' ? s_last_tx_type : "NONE",
                   (unsigned long)s_last_tx_len,
                   (unsigned long)idle_ms,
                   runtime_state_workflow_name(rs->workflow_state),
                   runtime_state_run_name(rs->run_state),
                   rs->ready ? 1U : 0U);
    bsp_debug_log(line);
}

void net_connectivity_get_last_disconnect_diag(net_disconnect_diag_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_last_disconnect_diag;
}

static int tcp_send_json_frame(const char *json_body, size_t json_len)
{
    int n;
    uint32_t prefix_len;

    if (json_body == NULL || json_len == 0U) {
        return -1;
    }
    if (json_len > NET_JSON_FRAME_MAX) {
        bsp_debug_log("[PROTO] outbound JSON exceeds 2048-byte single-frame limit\r\n");
        return -4;
    }
    n = proto_envelope_encode(json_body, json_len, s_wire, sizeof(s_wire));
    if (n < 0) {
        return -2;
    }
    prefix_len = ((uint32_t)s_wire[0] << 24) | ((uint32_t)s_wire[1] << 16) |
                 ((uint32_t)s_wire[2] << 8) | (uint32_t)s_wire[3];
    net_log_prefix(prefix_len, (size_t)n);
    net_log_json_tail(json_body, json_len);
    if (s_sock.connected == 0) {
        return -3;
    }
    net_note_tx_context(json_body, json_len, s_last_poll_ms);
    return net_socket_client_send(&s_sock, s_wire, (size_t)n);
}

static void net_try_register(uint32_t monotonic_ms)
{
    int n;
    int rc;
    const controller_identity_t *id;

    if (s_register_pending == 0U || s_register_inflight != 0U || s_sock.connected == 0) {
        return;
    }
    if (s_next_register_attempt_ms != 0U &&
        (int32_t)(monotonic_ms - s_next_register_attempt_ms) < 0) {
        return;
    }
    id = common_identity_get();
    if (id == NULL || id->imei[0] == '\0' || id->iccid[0] == '\0') {
        if (s_last_register_attempt_ms == 0U ||
            (uint32_t)(monotonic_ms - s_last_register_attempt_ms) >= NET_REGISTER_IDENTITY_RETRY_MS) {
            bsp_debug_log("[PROTO] REGISTER deferred: modem identity unavailable\r\n");
            s_last_register_attempt_ms = monotonic_ms;
        }
        s_next_register_attempt_ms = monotonic_ms + NET_REGISTER_IDENTITY_RETRY_MS;
        return;
    }

    n = proto_register_build(s_tx_json, sizeof(s_tx_json));
    if (n <= 0) {
        char line[96];
        (void)snprintf(line, sizeof(line), "[PROTO] REGISTER build failed rc=%d\r\n", n);
        bsp_debug_log(line);
        s_last_register_attempt_ms = monotonic_ms;
        net_register_retry_schedule(monotonic_ms, "build_failed");
        return;
    }
    net_log_json("TX", s_tx_json, (size_t)n);
    net_log_full_json("REGISTER", s_tx_json, (size_t)n);
    rc = tcp_send_json_frame(s_tx_json, (size_t)n);
    if (rc >= 0) {
        bsp_debug_log("[PROTO] REGISTER send success\r\n");
        s_register_inflight = 1U;
        s_register_pending = 0U;
        s_last_register_attempt_ms = monotonic_ms;
        net_register_retry_reset();
        common_status_set_registered(true);
        runtime_state_inc_counter_register_ok();
        if (runtime_state_get()->time_synced == false) {
            bsp_debug_log("[TIME] REGISTER sent without ts (time_synced=false)\r\n");
        }
    } else {
        bsp_debug_log("[PROTO] REGISTER send failed\r\n");
        s_last_register_attempt_ms = monotonic_ms;
        net_register_retry_schedule(monotonic_ms, "send_failed");
    }
}

void net_connectivity_force_reconnect(void)
{
    net_handle_disconnect("local_reset", s_last_poll_ms, 0U);
}

void net_connectivity_on_register_ack(void)
{
    common_status_set_registered(true);
    s_register_pending = 0U;
    s_register_inflight = 0U;
    net_register_retry_reset();
    bsp_debug_log("[PROTO] REGISTER_ACK received\r\n");
}

void net_connectivity_on_register_nack(void)
{
    common_status_set_registered(false);
    s_register_pending = 1U;
    s_register_inflight = 0U;
    net_register_retry_schedule(s_last_poll_ms, "register_nack");
    bsp_debug_log("[PROTO] REGISTER_NACK received\r\n");
}

void net_connectivity_init(void)
{
    bsp_rtc_reset();
    net_platform_config_init();
    net_socket_client_init(&s_sock);
    net_4g_modem_init();
    s_last_connect_attempt_ms = 0U;
    s_last_register_attempt_ms = 0U;
    s_next_register_attempt_ms = 0U;
    s_last_poll_ms = 0U;
    s_tcp_connected_since_ms = 0U;
    s_last_tx_ms = 0U;
    s_last_tx_len = 0U;
    s_register_pending = 1U;
    s_register_inflight = 0U;
    s_register_retry_stage = 0U;
    s_register_retry_prng = 0x5A17U;
    memset(s_last_tx_type, 0, sizeof(s_last_tx_type));
    common_status_set_online(net_4g_modem_is_online());
    common_status_set_tcp_connected(false);
    common_status_set_registered(false);
}

void net_connectivity_poll(uint32_t monotonic_ms)
{
    const net_platform_endpoint_t *ep;
    const common_status_t *status_snapshot;
    static uint8_t s_warmup_logged;
    uint8_t was_online;
    uint8_t modem_online;

    s_last_poll_ms = monotonic_ms;
    net_4g_modem_poll(monotonic_ms);
    status_snapshot = common_status_get();
    was_online = (status_snapshot != NULL && status_snapshot->online) ? 1U : 0U;
    modem_online = net_4g_modem_is_online() ? 1U : 0U;
    common_status_set_online(modem_online != 0U);

    if (modem_online == 0U) {
        if (s_sock.connected != 0 || was_online != 0U) {
            net_handle_disconnect("network_lost", monotonic_ms, 1U);
            return;
        }
        net_mark_disconnected();
        return;
    }
    if (s_sock.connected != 0 && net_4g_modem_tcp_is_connected() == 0) {
        net_log_disconnect_context("modem_closed", monotonic_ms);
        bsp_debug_log("[PROTO] TCP closed by peer/modem, reconnect pending\r\n");
        runtime_state_inc_counter_network_lost();
        net_mark_disconnected();
    }

    ep = net_platform_endpoint_get();
    if (s_sock.connected == 0 && net_4g_modem_is_online()) {
        uint32_t online_age_ms = net_4g_modem_online_age_ms(monotonic_ms);
        if (online_age_ms < NET_MODEM_WARMUP_MS) {
            if (s_warmup_logged == 0U) {
                char line[128];
                (void)snprintf(line, sizeof(line),
                               "[PROTO] TCP connect deferred: modem warmup %lu/%lu ms\r\n",
                               (unsigned long)online_age_ms,
                               (unsigned long)NET_MODEM_WARMUP_MS);
                bsp_debug_log(line);
                s_warmup_logged = 1U;
            }
            common_status_set_tcp_connected(false);
            return;
        }
    }
    s_warmup_logged = 0U;

    if (s_sock.connected == 0 && ep->tcp_host[0] != '\0' && ep->tcp_port != 0U &&
        (s_last_connect_attempt_ms == 0U ||
         (uint32_t)(monotonic_ms - s_last_connect_attempt_ms) >= NET_RECONNECT_BACKOFF_MS)) {
        s_last_connect_attempt_ms = monotonic_ms;
        if (net_socket_client_connect(&s_sock, ep->tcp_host, ep->tcp_port) == 0) {
            bsp_debug_log("[PROTO] TCP connect success\r\n");
            s_tcp_connected_since_ms = monotonic_ms;
            common_status_set_tcp_connected(true);
            s_register_pending = 1U;
            s_register_inflight = 0U;
        } else {
            bsp_debug_log("[PROTO] TCP connect failed\r\n");
        }
    }

    common_status_set_tcp_connected(s_sock.connected != 0);
    if (s_sock.connected == 0) {
        return;
    }

    net_try_register(monotonic_ms);

    for (uint32_t handled = 0U; handled < NET_MAX_INBOUND_FRAMES_PER_POLL; handled++) {
        size_t reply_len = 0U;
        size_t json_len = 0U;
        int feed_rc = net_socket_client_poll(&s_sock, monotonic_ms, s_rx_json, sizeof(s_rx_json), &json_len);
        if (feed_rc != 1) {
            break;
        }
        net_log_json("RX", s_rx_json, json_len);
        if (proto_dispatch_handle_inbound(s_rx_json, json_len, s_tx_json, sizeof(s_tx_json), &reply_len) < 0) {
            bsp_debug_log("[PROTO] inbound dispatch failed\r\n");
            continue;
        }
        if (reply_len > 0U) {
            net_log_json("TX", s_tx_json, reply_len);
            if (tcp_send_json_frame(s_tx_json, reply_len) >= 0) {
                bsp_debug_log("[PROTO] command reply send success\r\n");
            } else {
                bsp_debug_log("[PROTO] command reply send failed\r\n");
            }
        }
    }
}

int net_connectivity_send_json(const char *json_body, size_t json_len)
{
    int rc;

    if (json_body == NULL || json_len == 0U) {
        return -1;
    }
    rc = net_json_validate_business_frame(json_body, json_len);
    if (rc != 0) {
        bsp_debug_log("[PROTO] outbound JSON validation failed\r\n");
        return -2;
    }
    net_log_json("TX", json_body, json_len);
    rc = tcp_send_json_frame(json_body, json_len);
    if (rc < 0 && s_sock.connected != 0) {
        net_handle_disconnect(net_4g_modem_is_online() != 0 ? "peer_closed" : "network_lost",
                              s_last_poll_ms,
                              1U);
    }
    return rc;
}
