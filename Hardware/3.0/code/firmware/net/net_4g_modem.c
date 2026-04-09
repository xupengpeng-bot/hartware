#include "net_4g_modem.h"
#include "board_hw_config.h"
#include "bsp_system.h"
#include "bsp_uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* EC801 / Quectel LTE：UART4 + AT+QI* TCP（与 Quectel TCP/IP Application Note 一致）。 */

#if defined(BOARD_STM32F103)

#define RCC_BASE           0x40021000U
#define GPIOB_BASE         0x40010C00U
#define GPIOC_BASE         0x40011000U

#define RCC_APB2ENR        (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define GPIOB_CRL          (*((volatile uint32_t *)(GPIOB_BASE + 0x00U)))
#define GPIOB_BSRR         (*((volatile uint32_t *)(GPIOB_BASE + 0x10U)))
#define GPIOC_CRL          (*((volatile uint32_t *)(GPIOC_BASE + 0x00U)))
#define GPIOC_BSRR         (*((volatile uint32_t *)(GPIOC_BASE + 0x10U)))

#define RCC_APB2ENR_IOPBEN (1U << 3)
#define RCC_APB2ENR_IOPCEN (1U << 4)

/* F103RC RAM 紧张：URC 行解析与 TCP 载荷缓存尽量小；大帧依赖 net_socket_client 聚合。 */
#define MODEM_STREAM_CAP   384U
#define MODEM_LINE_MAX     256U
#define TCP_RX_FIFO_CAP    768U
#define TCP_CONNECT_ID     0U
#define PDP_CONTEXT_ID     1U

static uint8_t  s_rx_stream[MODEM_STREAM_CAP];
static size_t   s_rx_len;
static uint8_t  s_tcp_fifo[TCP_RX_FIFO_CAP];
static size_t   s_tcp_head;
static size_t   s_tcp_tail;
static int      s_online;
static int      s_tcp_connected;

static void modem_power_gpio_init(void)
{
    uint32_t crl;
    const uint32_t shift = (uint32_t)BOARD_HW_PIN_NET_POWER_PORT_C * 4U;

    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    crl = GPIOC_CRL;
    crl &= ~(0xFU << shift);
    crl |= (0x3U << shift);
    GPIOC_CRL = crl;
}

static void modem_power_set(int level)
{
    if (level != 0) {
        GPIOC_BSRR = (1U << BOARD_HW_PIN_NET_POWER_PORT_C);
    } else {
        GPIOC_BSRR = (1U << (BOARD_HW_PIN_NET_POWER_PORT_C + 16U));
    }
}

static void modem_pwrkey_gpio_init(void)
{
    uint32_t crl;
    /* PB4 -> CRL bits [19:16] = pin * 4 (was wrongly using offset 20 = PB5). */
    const uint32_t shift = (uint32_t)BOARD_HW_PIN_NET_PWRKEY_PORT_B * 4U;

    RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;

    crl = GPIOB_CRL;
    crl &= ~(0xFU << shift);
    crl |= (0x3U << shift);
    GPIOB_CRL = crl;

    GPIOB_BSRR = (1U << (BOARD_HW_PIN_NET_PWRKEY_PORT_B + 16U));
}

static void modem_pwrkey_set(int level)
{
    if (level != 0) {
        GPIOB_BSRR = (1U << BOARD_HW_PIN_NET_PWRKEY_PORT_B);
    } else {
        GPIOB_BSRR = (1U << (BOARD_HW_PIN_NET_PWRKEY_PORT_B + 16U));
    }
}

#else

static void modem_power_gpio_init(void) {}

static void modem_power_set(int level)
{
    (void)level;
}

static void modem_pwrkey_gpio_init(void) {}

static void modem_pwrkey_set(int level)
{
    (void)level;
}

#endif

static void modem_drain_hw_rx(void)
{
    uint8_t ch;
    while (bsp_uart_read((int)BOARD_HW_UART_PORT_MODEM_4G, &ch, 1U) > 0) {
    }
}

static void stream_push_from_uart(void)
{
    uint8_t ch;
    while (bsp_uart_read((int)BOARD_HW_UART_PORT_MODEM_4G, &ch, 1U) > 0) {
        if (s_rx_len < MODEM_STREAM_CAP) {
            s_rx_stream[s_rx_len++] = ch;
        } else {
            memmove(s_rx_stream, s_rx_stream + 1U, MODEM_STREAM_CAP - 1U);
            s_rx_stream[MODEM_STREAM_CAP - 1U] = ch;
        }
    }
}

static int modem_getch_ms(uint32_t timeout_ms)
{
    uint32_t waited = 0U;
    while (waited < timeout_ms) {
        stream_push_from_uart();
        if (s_rx_len > 0U) {
            uint8_t b = s_rx_stream[0];
            memmove(s_rx_stream, s_rx_stream + 1U, s_rx_len - 1U);
            s_rx_len--;
            return (int)b;
        }
        bsp_system_delay_ms(1U);
        waited++;
    }
    return -1;
}

static void tcp_fifo_push(const uint8_t *p, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        size_t next = (s_tcp_tail + 1U) % TCP_RX_FIFO_CAP;
        if (next == s_tcp_head) {
            s_tcp_head = (s_tcp_head + 1U) % TCP_RX_FIFO_CAP;
        }
        s_tcp_fifo[s_tcp_tail] = p[i];
        s_tcp_tail = next;
    }
}

size_t net_4g_modem_tcp_rx_pop(uint8_t *buf, size_t cap)
{
    if (buf == NULL || cap == 0U) {
        return 0U;
    }
    size_t n = 0U;
    while (n < cap && s_tcp_head != s_tcp_tail) {
        buf[n++] = s_tcp_fifo[s_tcp_head];
        s_tcp_head = (s_tcp_head + 1U) % TCP_RX_FIFO_CAP;
    }
    return n;
}

static int modem_write_str(const char *s)
{
    if (s == NULL) {
        return -1;
    }
    size_t len = strlen(s);
    return bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, (const uint8_t *)s, len);
}

static int modem_wait_substr(const char *needle, uint32_t timeout_ms)
{
    char     win[48];
    size_t   wlen = 0U;
    uint32_t waited = 0U;

    if (needle == NULL) {
        return -1;
    }
    memset(win, 0, sizeof(win));

    while (waited < timeout_ms) {
        int ch = modem_getch_ms(1U);
        if (ch < 0) {
            waited++;
            continue;
        }
        if (wlen + 1U >= sizeof(win)) {
            memmove(win, win + 1U, sizeof(win) - 2U);
            wlen = sizeof(win) - 2U;
        }
        win[wlen++] = (char)ch;
        win[wlen] = '\0';
        if (strstr(win, needle) != NULL) {
            return 0;
        }
    }
    return -1;
}

static int modem_wait_line_ok_or_err(uint32_t timeout_ms)
{
    char     line[MODEM_LINE_MAX];
    size_t   li = 0U;
    uint32_t waited = 0U;

    while (waited < timeout_ms) {
        int ch = modem_getch_ms(1U);
        if (ch < 0) {
            waited++;
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[li] = '\0';
            if (strstr(line, "OK") != NULL) {
                return 0;
            }
            if (strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) {
                return -1;
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }
    return -1;
}

static int modem_at_simple_ok(const char *cmd, uint32_t timeout_ms)
{
    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_write_str(cmd) < 0) {
        return -1;
    }
    return modem_wait_line_ok_or_err(timeout_ms);
}

static int modem_wait_qiopen_result(uint32_t timeout_ms)
{
    char     line[MODEM_LINE_MAX];
    size_t   li = 0U;
    uint32_t waited = 0U;

    while (waited < timeout_ms) {
        int ch = modem_getch_ms(1U);
        if (ch < 0) {
            waited++;
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[li] = '\0';
            if (strstr(line, "+QIOPEN:") != NULL) {
                unsigned cid = 0U;
                int      err = -1;
                if (sscanf(line, "+QIOPEN: %u,%d", &cid, &err) == 2 && (int)cid == TCP_CONNECT_ID && err == 0) {
                    return 0;
                }
                return -1;
            }
            if (strstr(line, "OK") != NULL) {
                li = 0U;
                continue;
            }
            if (strstr(line, "ERROR") != NULL) {
                return -1;
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }
    return -1;
}

static int modem_qird_fetch(unsigned request_len)
{
    char    cmd[48];
    char    line[MODEM_LINE_MAX];
    int     n_data = -1;

    if (request_len == 0U) {
        return 0;
    }
    /* 单次读取与 TCP 缓存匹配，剩余数据由下一次 +QIURC 再拉取 */
    if (request_len > 512U) {
        request_len = 512U;
    }
    (void)snprintf(cmd, sizeof(cmd), "AT+QIRD=%u,%u\r\n", TCP_CONNECT_ID, (unsigned)request_len);
    if (modem_write_str(cmd) < 0) {
        return -1;
    }

    for (;;) {
        size_t li = 0U;
        for (;;) {
            int ch = modem_getch_ms(8000U);
            if (ch < 0) {
                return -1;
            }
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                line[li] = '\0';
                break;
            }
            if (li + 1U < sizeof(line)) {
                line[li++] = (char)ch;
            }
        }

        if (strncmp(line, "+QIRD:", 6) == 0) {
            int a = 0;
            int b = 0;
            int c = 0;
            if (sscanf(line, "+QIRD: %d,%d,%d", &a, &b, &c) == 3) {
                n_data = b;
            } else if (sscanf(line, "+QIRD: %d", &a) == 1) {
                n_data = a;
            } else {
                return -1;
            }
            break;
        }
        if (strstr(line, "ERROR") != NULL) {
            return -1;
        }
    }

    if (n_data <= 0) {
        for (;;) {
            int ch = modem_getch_ms(2000U);
            if (ch < 0) {
                break;
            }
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                break;
            }
        }
        return 0;
    }

    {
        uint8_t tmp[256];
        int     remain = n_data;
        while (remain > 0) {
            int chunk = remain > (int)sizeof(tmp) ? (int)sizeof(tmp) : remain;
            for (int i = 0; i < chunk; i++) {
                int ch = modem_getch_ms(8000U);
                if (ch < 0) {
                    return -1;
                }
                tmp[i] = (uint8_t)ch;
            }
            tcp_fifo_push(tmp, (size_t)chunk);
            remain -= chunk;
        }
    }

    (void)modem_wait_line_ok_or_err(3000U);
    return n_data;
}

static void handle_urc_line(const char *line)
{
    if (line == NULL) {
        return;
    }
    if (strstr(line, "+QIURC: \"closed\"") != NULL) {
        s_tcp_connected = 0;
        bsp_debug_log("[4G] TCP closed by peer\r\n");
        return;
    }
    if (strstr(line, "+QIURC: \"recv\"") == NULL) {
        return;
    }

    unsigned recv_len = 512U;
    {
        const char *p = strstr(line, "\"recv\"");
        if (p != NULL) {
            p = strchr(p, ',');
            if (p != NULL) {
                p++;
                (void)strtoul(p, NULL, 10);
                const char *p2 = strchr(p, ',');
                if (p2 != NULL) {
                    p2++;
                    recv_len = (unsigned)strtoul(p2, NULL, 10);
                    if (recv_len == 0U || recv_len > 512U) {
                        recv_len = 512U;
                    }
                }
            }
        }
    }
    (void)modem_qird_fetch(recv_len);
}

static void modem_process_stream_lines(void)
{
    while (s_rx_len > 0U) {
        size_t i = 0U;
        for (; i < s_rx_len; i++) {
            if (s_rx_stream[i] == (uint8_t)'\n') {
                break;
            }
        }
        if (i >= s_rx_len) {
            break;
        }
        size_t line_end = i;
        size_t line_start = 0U;
        size_t line_len = line_end - line_start;
        if (line_len > 0U && s_rx_stream[line_end - 1U] == (uint8_t)'\r') {
            line_len--;
        }
        char line[MODEM_LINE_MAX];
        if (line_len >= sizeof(line)) {
            line_len = sizeof(line) - 1U;
        }
        memcpy(line, s_rx_stream, line_len);
        line[line_len] = '\0';
        size_t consume = i + 1U;
        memmove(s_rx_stream, s_rx_stream + consume, s_rx_len - consume);
        s_rx_len -= consume;

        if (line_len > 0U) {
            handle_urc_line(line);
        }
    }
}

static int modem_probe_online(void)
{
    static const char at_cmd[] = "AT\r\n";

    for (int attempt = 0; attempt < 5; ++attempt) {
        modem_drain_hw_rx();
        s_rx_len = 0U;
        (void)bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, (const uint8_t *)at_cmd, sizeof(at_cmd) - 1U);
        if (modem_wait_substr("OK", 1200U) == 0) {
            return 1;
        }
        bsp_system_delay_ms(500U);
    }
    return 0;
}

void net_4g_modem_init(void)
{
    s_online = 0;
    s_tcp_connected = 0;
    s_rx_len = 0U;
    s_tcp_head = 0U;
    s_tcp_tail = 0U;

    bsp_debug_log("[4G] init start\r\n");
    bsp_uart_modem_init();
    modem_power_gpio_init();
    modem_pwrkey_gpio_init();
    modem_power_set(1);
    bsp_debug_log("[4G] power enabled\r\n");
    bsp_system_delay_ms(200U);
    modem_drain_hw_rx();

    modem_pwrkey_set(1);
    bsp_system_delay_ms(1200U);
    modem_pwrkey_set(0);

    bsp_debug_log("[4G] PWRKEY pulse sent, waiting for boot\r\n");
    bsp_system_delay_ms(5000U);

    /* 飞行模式/省电态时先拉满射频再探测 */
    (void)modem_at_simple_ok("AT+CFUN=1\r\n", 25000U);
    bsp_system_delay_ms(2000U);
    modem_drain_hw_rx();
    s_rx_len = 0U;

    if (modem_probe_online() != 0) {
        s_online = 1;
        bsp_debug_log("[4G] modem responded to AT\r\n");
        (void)modem_at_simple_ok("ATE0\r\n", 3000U);
    } else {
        bsp_debug_log("[4G] modem did not respond to AT (poll will retry)\r\n");
    }
}

static void modem_offline_recovery(uint32_t monotonic_ms)
{
    static uint32_t s_last_recovery_ms;
    static uint8_t  s_cfun_once;

    if (s_online != 0) {
        return;
    }
    if (monotonic_ms < 8000U) {
        return;
    }
    if (s_last_recovery_ms != 0U && (monotonic_ms - s_last_recovery_ms) < 20000U) {
        return;
    }
    s_last_recovery_ms = monotonic_ms;

    bsp_debug_log("[4G] offline recovery try\r\n");
    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_probe_online() != 0) {
        s_online = 1;
        bsp_debug_log("[4G] modem OK (recovery)\r\n");
        (void)modem_at_simple_ok("ATE0\r\n", 3000U);
        return;
    }
    if (s_cfun_once == 0U) {
        s_cfun_once = 1U;
        (void)modem_at_simple_ok("AT+CFUN=1\r\n", 25000U);
        bsp_system_delay_ms(3000U);
        modem_drain_hw_rx();
        s_rx_len = 0U;
        if (modem_probe_online() != 0) {
            s_online = 1;
            bsp_debug_log("[4G] modem OK after CFUN (recovery)\r\n");
            (void)modem_at_simple_ok("ATE0\r\n", 3000U);
        }
    }
}

bool net_4g_modem_is_online(void)
{
    return s_online != 0;
}

int net_4g_modem_tcp_connect(const char *host, uint16_t port)
{
#if !defined(BOARD_STM32F103)
    (void)host;
    (void)port;
    return -1;
#else
    if (host == NULL || host[0] == '\0' || port == 0U || s_online == 0) {
        return -1;
    }

    modem_drain_hw_rx();
    s_rx_len = 0U;

    bsp_debug_log("[4G] TCP: QICSGP\r\n");
    if (modem_at_simple_ok("AT+QICSGP=1,1,\"\",\"\",\"\",1\r\n", 5000U) != 0) {
        bsp_debug_log("[4G] TCP: QICSGP failed\r\n");
        return -1;
    }

    bsp_debug_log("[4G] TCP: QIACT\r\n");
    if (modem_at_simple_ok("AT+QIACT=1\r\n", 45000U) != 0) {
        bsp_debug_log("[4G] TCP: QIACT failed (SIM/APN?)\r\n");
        return -1;
    }

    (void)modem_at_simple_ok("AT+QICLOSE=0\r\n", 8000U);

    {
        char cmd[180];
        int  n = snprintf(cmd, sizeof(cmd), "AT+QIOPEN=1,%u,\"TCP\",\"%s\",%u\r\n", TCP_CONNECT_ID, host, (unsigned)port);
        if (n <= 0 || (size_t)n >= sizeof(cmd)) {
            bsp_debug_log("[4G] TCP: host too long\r\n");
            return -1;
        }
        bsp_debug_log("[4G] TCP: QIOPEN\r\n");
        modem_drain_hw_rx();
        s_rx_len = 0U;
        if (modem_write_str(cmd) < 0) {
            return -1;
        }
        if (modem_wait_qiopen_result(25000U) != 0) {
            bsp_debug_log("[4G] TCP: QIOPEN failed\r\n");
            return -1;
        }
    }

    s_tcp_connected = 1;
    bsp_debug_log("[4G] TCP: connected\r\n");
    return 0;
#endif
}

void net_4g_modem_tcp_close(void)
{
#if defined(BOARD_STM32F103)
    s_tcp_connected = 0;
    modem_drain_hw_rx();
    s_rx_len = 0U;
    (void)modem_at_simple_ok("AT+QICLOSE=0\r\n", 8000U);
#endif
}

int net_4g_modem_tcp_send(const uint8_t *data, size_t len)
{
#if !defined(BOARD_STM32F103)
    (void)data;
    (void)len;
    return -1;
#else
    char hdr[48];

    if (data == NULL || len == 0U || s_tcp_connected == 0) {
        return -1;
    }

    if (len > 1460U) {
        len = 1460U;
    }

    modem_drain_hw_rx();
    s_rx_len = 0U;

    (void)snprintf(hdr, sizeof(hdr), "AT+QISEND=%u,%u\r\n", TCP_CONNECT_ID, (unsigned)len);
    if (modem_write_str(hdr) < 0) {
        return -1;
    }

    if (modem_wait_substr(">", 5000U) != 0) {
        bsp_debug_log("[4G] TCP: QISEND no prompt\r\n");
        return -1;
    }

    if (bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, data, len) != (int)len) {
        return -1;
    }

    if (modem_wait_substr("SEND OK", 15000U) != 0) {
        if (modem_wait_line_ok_or_err(3000U) != 0) {
            bsp_debug_log("[4G] TCP: QISEND failed\r\n");
            return -1;
        }
    }

    return (int)len;
#endif
}

int net_4g_modem_tcp_is_connected(void)
{
    return s_tcp_connected;
}

void net_4g_modem_poll(uint32_t monotonic_ms)
{
    stream_push_from_uart();
    modem_process_stream_lines();
    modem_offline_recovery(monotonic_ms);
}
