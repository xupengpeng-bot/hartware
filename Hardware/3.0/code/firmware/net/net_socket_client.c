#include "net_socket_client.h"
#include "proto_envelope.h"
#include "bsp_system.h"
#include "bsp_uart.h"
#include "board_hw_config.h"

#include <stdio.h>
#include <string.h>

#define NET_MODEM_UART_PORT            ((int)BOARD_HW_UART_PORT_MODEM_4G)
#define NET_SOCKET_ID_DEFAULT          0U
#define NET_AT_LINE_MAX                192U
#define NET_QIRD_CHUNK_MAX             1024U
#define NET_UART_POLL_DELAY_MS         10U
#define NET_AT_TIMEOUT_SHORT_MS        2000U
#define NET_AT_TIMEOUT_MEDIUM_MS       8000U
#define NET_AT_TIMEOUT_CONNECT_MS      30000U
#define NET_AT_TIMEOUT_SEND_MS         15000U

static void net_debug_log_line(const char *prefix, const char *line)
{
    if (prefix != NULL) {
        bsp_debug_log(prefix);
    }
    if (line != NULL) {
        bsp_debug_log(line);
    }
    bsp_debug_log("\r\n");
}

static int uart_write_all(const uint8_t *data, size_t len)
{
    return bsp_uart_write(NET_MODEM_UART_PORT, data, len);
}

static void uart_drain_rx(void)
{
    uint8_t ch = 0U;
    while (bsp_uart_read(NET_MODEM_UART_PORT, &ch, 1U) > 0) {
    }
}

static int uart_read_byte_timeout(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t waited = 0U;
    if (out == NULL) {
        return 0;
    }
    while (waited < timeout_ms) {
        if (bsp_uart_read(NET_MODEM_UART_PORT, out, 1U) > 0) {
            return 1;
        }
        bsp_system_delay_ms(NET_UART_POLL_DELAY_MS);
        waited += NET_UART_POLL_DELAY_MS;
    }
    return 0;
}

static int read_line_timeout(char *line, size_t cap, uint32_t timeout_ms)
{
    uint32_t waited = 0U;
    size_t   len = 0U;
    uint8_t  ch = 0U;

    if (line == NULL || cap < 2U) {
        return 0;
    }
    line[0] = '\0';

    while (waited < timeout_ms) {
        if (bsp_uart_read(NET_MODEM_UART_PORT, &ch, 1U) > 0) {
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                if (len == 0U) {
                    continue;
                }
                line[len] = '\0';
                return 1;
            }
            if (len + 1U < cap) {
                line[len++] = (char)ch;
                line[len] = '\0';
            }
            continue;
        }
        bsp_system_delay_ms(NET_UART_POLL_DELAY_MS);
        waited += NET_UART_POLL_DELAY_MS;
    }

    if (len > 0U) {
        line[len] = '\0';
        return 1;
    }
    return 0;
}

static int line_is_ok(const char *line)
{
    return line != NULL && strcmp(line, "OK") == 0;
}

static int line_is_error(const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }
    return strcmp(line, "ERROR") == 0 || strncmp(line, "+CME ERROR", 10) == 0 || strncmp(line, "+CMS ERROR", 10) == 0;
}

static int wait_for_ok_or_error(uint32_t timeout_ms)
{
    char line[NET_AT_LINE_MAX];
    while (read_line_timeout(line, sizeof(line), timeout_ms) == 1) {
        if (line_is_ok(line)) {
            return 1;
        }
        if (line_is_error(line)) {
            net_debug_log_line("[NET] AT error: ", line);
            return -1;
        }
    }
    return 0;
}

static int send_at_command_simple(const char *cmd, uint32_t timeout_ms, int ignore_error)
{
    if (cmd == NULL) {
        return -1;
    }
    (void)uart_write_all((const uint8_t *)cmd, strlen(cmd));
    {
        int rc = wait_for_ok_or_error(timeout_ms);
        if (rc > 0) {
            return 0;
        }
        if (ignore_error != 0) {
            return 0;
        }
        return -1;
    }
}

static int wait_for_prompt(uint32_t timeout_ms)
{
    uint32_t waited = 0U;
    uint8_t  ch = 0U;
    char     line[NET_AT_LINE_MAX];
    size_t   line_len = 0U;

    memset(line, 0, sizeof(line));
    while (waited < timeout_ms) {
        if (bsp_uart_read(NET_MODEM_UART_PORT, &ch, 1U) > 0) {
            if (ch == '>') {
                return 1;
            }
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                if (line_len > 0U) {
                    line[line_len] = '\0';
                    if (line_is_error(line)) {
                        net_debug_log_line("[NET] prompt error: ", line);
                        return -1;
                    }
                    line_len = 0U;
                    line[0] = '\0';
                }
                continue;
            }
            if (line_len + 1U < sizeof(line)) {
                line[line_len++] = (char)ch;
                line[line_len] = '\0';
            }
            continue;
        }
        bsp_system_delay_ms(NET_UART_POLL_DELAY_MS);
        waited += NET_UART_POLL_DELAY_MS;
    }
    return 0;
}

static int wait_for_send_result(uint32_t timeout_ms)
{
    char line[NET_AT_LINE_MAX];
    while (read_line_timeout(line, sizeof(line), timeout_ms) == 1) {
        if (strcmp(line, "SEND OK") == 0) {
            return 1;
        }
        if (strcmp(line, "SEND FAIL") == 0 || line_is_error(line)) {
            net_debug_log_line("[NET] send failed: ", line);
            return -1;
        }
    }
    return 0;
}

static int wait_for_qiopen_result(uint8_t socket_id, uint32_t timeout_ms)
{
    char line[NET_AT_LINE_MAX];
    int  seen_ok = 0;
    while (read_line_timeout(line, sizeof(line), timeout_ms) == 1) {
        unsigned parsed_id = 0U;
        unsigned err = 0U;
        if (line_is_ok(line)) {
            seen_ok = 1;
            continue;
        }
        if (sscanf(line, "+QIOPEN: %u,%u", &parsed_id, &err) == 2) {
            if (parsed_id == (unsigned)socket_id && err == 0U) {
                return 1;
            }
            net_debug_log_line("[NET] QIOPEN failed: ", line);
            return -1;
        }
        if (strstr(line, "ALREADY CONNECT") != NULL) {
            return 1;
        }
        if (line_is_error(line)) {
            net_debug_log_line("[NET] QIOPEN error: ", line);
            return seen_ok ? -1 : -1;
        }
    }
    return 0;
}

static int read_exact_bytes(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t   got = 0U;
    uint32_t idle = 0U;
    if (buf == NULL && len > 0U) {
        return -1;
    }
    while (got < len && idle < timeout_ms) {
        int rc = bsp_uart_read(NET_MODEM_UART_PORT, &buf[got], 1U);
        if (rc > 0) {
            got += (size_t)rc;
            idle = 0U;
            continue;
        }
        bsp_system_delay_ms(NET_UART_POLL_DELAY_MS);
        idle += NET_UART_POLL_DELAY_MS;
    }
    return got == len ? 0 : -1;
}

static int qird_read_bytes(net_socket_client_t *c, uint16_t requested, uint8_t *out, size_t out_cap)
{
    char cmd[32];
    char line[NET_AT_LINE_MAX];
    unsigned available = 0U;

    if (c == NULL || out == NULL || out_cap == 0U) {
        return -1;
    }

    if (requested == 0U) {
        requested = (uint16_t)out_cap;
    }

    (void)snprintf(cmd, sizeof(cmd), "AT+QIRD=%u,%u\r\n", (unsigned)c->socket_id, (unsigned)requested);
    (void)uart_write_all((const uint8_t *)cmd, strlen(cmd));

    while (read_line_timeout(line, sizeof(line), NET_AT_TIMEOUT_MEDIUM_MS) == 1) {
        if (sscanf(line, "+QIRD: %u", &available) == 1) {
            size_t read_len = available > out_cap ? out_cap : (size_t)available;
            if (read_len > 0U && read_exact_bytes(out, read_len, NET_AT_TIMEOUT_MEDIUM_MS) != 0) {
                return -1;
            }
            if ((size_t)available > read_len) {
                uint8_t sink[32];
                size_t remaining = (size_t)available - read_len;
                while (remaining > 0U) {
                    size_t chunk = remaining > sizeof(sink) ? sizeof(sink) : remaining;
                    if (read_exact_bytes(sink, chunk, NET_AT_TIMEOUT_MEDIUM_MS) != 0) {
                        return -1;
                    }
                    remaining -= chunk;
                }
            }
            if (wait_for_ok_or_error(NET_AT_TIMEOUT_SHORT_MS) < 0) {
                return -1;
            }
            return (int)read_len;
        }
        if (line_is_error(line)) {
            net_debug_log_line("[NET] QIRD error: ", line);
            return -1;
        }
    }
    return -1;
}

static int parse_recv_urc(const char *line, uint8_t socket_id, uint16_t *out_len)
{
    unsigned parsed_socket = 0U;
    unsigned parsed_len = 0U;

    if (line == NULL) {
        return 0;
    }
    if (sscanf(line, "+QIURC: \"recv\",%u,%u", &parsed_socket, &parsed_len) >= 1) {
        if (parsed_socket != (unsigned)socket_id) {
            return 0;
        }
        if (out_len != NULL) {
            *out_len = (uint16_t)(parsed_len > 0xFFFFU ? 0xFFFFU : parsed_len);
        }
        return 1;
    }
    return 0;
}

static int process_async_line(net_socket_client_t *c, const char *line,
                              char *out_json, size_t out_cap, size_t *out_json_len)
{
    uint16_t recv_len = 0U;

    if (c == NULL || line == NULL) {
        return 0;
    }

    if (parse_recv_urc(line, c->socket_id, &recv_len) == 1) {
        uint8_t raw[NET_QIRD_CHUNK_MAX];
        if (recv_len == 0U) {
            recv_len = NET_QIRD_CHUNK_MAX;
        }
        while (recv_len > 0U) {
            uint16_t want = recv_len > NET_QIRD_CHUNK_MAX ? NET_QIRD_CHUNK_MAX : recv_len;
            int got = qird_read_bytes(c, want, raw, sizeof(raw));
            if (got <= 0) {
                break;
            }
            if (net_socket_client_feed(c, raw, (size_t)got, out_json, out_cap, out_json_len) == 1) {
                return 1;
            }
            if ((uint16_t)got >= recv_len) {
                break;
            }
            recv_len = (uint16_t)(recv_len - (uint16_t)got);
        }
        return 0;
    }

    if (strstr(line, "+QIURC: \"closed\"") != NULL || strcmp(line, "CLOSED") == 0) {
        net_debug_log_line("[NET] socket closed: ", line);
        c->connected = 0;
        return 0;
    }
    if (strstr(line, "+QIURC: \"pdpdeact\"") != NULL) {
        net_debug_log_line("[NET] PDP deactivated: ", line);
        c->connected = 0;
        return 0;
    }
    return 0;
}

void net_socket_client_init(net_socket_client_t *c)
{
    if (!c) {
        return;
    }
    memset(c->rx, 0, sizeof(c->rx));
    c->rx_len = 0U;
    c->connected = 0;
    c->socket_id = NET_SOCKET_ID_DEFAULT;
    c->host[0] = '\0';
    c->port = 0U;
}

int net_socket_client_connect(net_socket_client_t *c, const char *host, uint16_t port)
{
    char cmd[192];

    if (!c || host == NULL || host[0] == '\0' || port == 0U) {
        return -1;
    }

    (void)strncpy(c->host, host, sizeof(c->host) - 1U);
    c->host[sizeof(c->host) - 1U] = '\0';
    c->port = port;
    c->socket_id = NET_SOCKET_ID_DEFAULT;

    uart_drain_rx();
    (void)send_at_command_simple("ATE0\r\n", NET_AT_TIMEOUT_SHORT_MS, 1);
    (void)send_at_command_simple("AT+QICLOSE=0\r\n", NET_AT_TIMEOUT_SHORT_MS, 1);
    (void)send_at_command_simple("AT+QIACT=1\r\n", NET_AT_TIMEOUT_MEDIUM_MS, 1);

    (void)snprintf(cmd, sizeof(cmd), "AT+QIOPEN=1,%u,\"TCP\",\"%s\",%u,0,1\r\n",
                   (unsigned)c->socket_id, c->host, (unsigned)c->port);
    (void)uart_write_all((const uint8_t *)cmd, strlen(cmd));

    if (wait_for_qiopen_result(c->socket_id, NET_AT_TIMEOUT_CONNECT_MS) <= 0) {
        c->connected = 0;
        return -2;
    }

    c->connected = 1;
    return 0;
}

void net_socket_client_disconnect(net_socket_client_t *c)
{
    if (!c) {
        return;
    }
    if (c->connected != 0) {
        char cmd[24];
        (void)snprintf(cmd, sizeof(cmd), "AT+QICLOSE=%u\r\n", (unsigned)c->socket_id);
        (void)send_at_command_simple(cmd, NET_AT_TIMEOUT_SHORT_MS, 1);
    }
    c->connected = 0;
    c->rx_len = 0U;
}

int net_socket_client_send(net_socket_client_t *c, const uint8_t *data, size_t len)
{
    char cmd[32];

    if (!c || !data || len == 0U || c->connected == 0) {
        return -1;
    }
    if (len > 0xFFFFU) {
        return -2;
    }

    (void)snprintf(cmd, sizeof(cmd), "AT+QISEND=%u,%u\r\n", (unsigned)c->socket_id, (unsigned)len);
    (void)uart_write_all((const uint8_t *)cmd, strlen(cmd));
    if (wait_for_prompt(NET_AT_TIMEOUT_SHORT_MS) <= 0) {
        c->connected = 0;
        return -3;
    }
    if (uart_write_all(data, len) != (int)len) {
        c->connected = 0;
        return -4;
    }
    if (wait_for_send_result(NET_AT_TIMEOUT_SEND_MS) <= 0) {
        c->connected = 0;
        return -5;
    }
    return (int)len;
}

static void shift_left(net_socket_client_t *c, size_t n)
{
    if (n >= c->rx_len) {
        c->rx_len = 0U;
        return;
    }
    memmove(c->rx, c->rx + n, c->rx_len - n);
    c->rx_len -= n;
}

int net_socket_client_feed(net_socket_client_t *c, const uint8_t *chunk, size_t chunk_len,
                          char *out_json, size_t out_cap, size_t *out_json_len)
{
    if (out_json_len) {
        *out_json_len = 0U;
    }
    if (!c || !out_json || out_cap == 0U) {
        return 0;
    }
    if (chunk != NULL && chunk_len > 0U) {
        if (c->rx_len + chunk_len > sizeof(c->rx)) {
            c->rx_len = 0U;
            return 0;
        }
        memcpy(c->rx + c->rx_len, chunk, chunk_len);
        c->rx_len += chunk_len;
    }
    if (c->rx_len < PROTO_LENGTH_PREFIX_BYTES) {
        return 0;
    }
    uint32_t plen = ((uint32_t)c->rx[0] << 24) | ((uint32_t)c->rx[1] << 16) | ((uint32_t)c->rx[2] << 8) |
                    (uint32_t)c->rx[3];
    size_t frame = PROTO_LENGTH_PREFIX_BYTES + (size_t)plen;
    if (plen > sizeof(c->rx) || frame > sizeof(c->rx)) {
        shift_left(c, 1U);
        return 0;
    }
    if (c->rx_len < frame) {
        return 0;
    }
    if (plen + 1U > out_cap) {
        shift_left(c, frame);
        return 0;
    }
    memcpy(out_json, c->rx + PROTO_LENGTH_PREFIX_BYTES, plen);
    out_json[plen] = '\0';
    if (out_json_len) {
        *out_json_len = plen;
    }
    shift_left(c, frame);
    return 1;
}

int net_socket_client_poll(net_socket_client_t *c, uint32_t monotonic_ms,
                           char *out_json, size_t out_cap, size_t *out_json_len)
{
    char line[NET_AT_LINE_MAX];

    (void)monotonic_ms;
    if (out_json_len != NULL) {
        *out_json_len = 0U;
    }
    if (c == NULL || c->connected == 0) {
        return 0;
    }

    if (net_socket_client_feed(c, NULL, 0U, out_json, out_cap, out_json_len) == 1) {
        return 1;
    }

    while (read_line_timeout(line, sizeof(line), 20U) == 1) {
        if (process_async_line(c, line, out_json, out_cap, out_json_len) == 1) {
            return 1;
        }
        if (c->connected == 0) {
            return 0;
        }
    }

    return net_socket_client_feed(c, NULL, 0U, out_json, out_cap, out_json_len);
}
