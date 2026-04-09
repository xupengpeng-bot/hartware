#include "net_4g_modem.h"
#include "fw_build_config.h"
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
#define RCC_APB2ENR_AFIOEN (1U << 0)

#define AFIO_BASE   0x40010000U
#define AFIO_MAPR   (*((volatile uint32_t *)(AFIO_BASE + 0x04U)))
/* SWJ_CFG=010：关闭 JTAG，保留 SWD(PA13/14)；PB3/PB4/PA15 作普通 GPIO。PB4=NET_PWRKEY 否则为 NJTRST 无法开机 */
#define AFIO_MAPR_SWJ_CFG_MASK (7U << 24)
#define AFIO_MAPR_SWJ_CFG_JTAG_OFF_SWD_ON (2U << 24)

/* 与参考工程 `机井3.0一体机带lora_APP清洁工程` 中 NETWORK_POWERKEY_HOLD_MS（5×delay_ms(1000)）一致 */
#define MODEM_PWRKEY_HOLD_MS 5000U
/* VccOff≈5s 略缩短为 4s；VccOn 后 2s + 拉键前再等 2s，与参考工程 network_task 一致 */
#define MODEM_VCC_OFF_MS           5000U
#define MODEM_VCC_ON_SETTLE_MS     2000U
#define MODEM_PRE_PWRKEY_MS        2000U
#define MODEM_POST_PWRKEY_BOOT_MS  0U
/* 重试：仍做掉电再上电，延时略短以加快二次尝试 */
#define MODEM_RETRY_VCC_OFF_MS     3000U
#define MODEM_RETRY_VCC_ON_MS      1500U
#define MODEM_RETRY_PRE_KEY_MS     1500U

/* F103RC RAM 紧张：URC 行解析与 TCP 载荷缓存尽量小；大帧依赖 net_socket_client 聚合。 */
#define MODEM_STREAM_CAP   376U
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

static void tcp_fifo_push(const uint8_t *p, size_t n);
static void modem_log_at_tx(const char *cmd);
static void modem_log_tcp_tx(const uint8_t *data, size_t len);
static void modem_log_tcp_rx(const uint8_t *data, size_t len);

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

static void modem_pwrkey_set(int level)
{
    if (level != 0) {
        GPIOB_BSRR = (1U << BOARD_HW_PIN_NET_PWRKEY_PORT_B);
    } else {
        GPIOB_BSRR = (1U << (BOARD_HW_PIN_NET_PWRKEY_PORT_B + 16U));
    }
}

/* inverted=1：经三极管时 MCU 高电平为有效脉冲；=0：MCU 直连 Quectel 时低脉冲有效 */
static void modem_pwrkey_idle_inverted(int inverted)
{
    if (inverted) {
        GPIOB_BSRR = (1U << (BOARD_HW_PIN_NET_PWRKEY_PORT_B + 16U));
    } else {
        GPIOB_BSRR = (1U << BOARD_HW_PIN_NET_PWRKEY_PORT_B);
    }
}

static void modem_pwrkey_run_pulse_inverted(int inverted, uint32_t hold_ms)
{
    modem_pwrkey_idle_inverted(inverted);
    bsp_system_delay_ms(50U);
    if (inverted) {
        modem_pwrkey_set(1);
        bsp_system_delay_ms(hold_ms);
        modem_pwrkey_set(0);
    } else {
        modem_pwrkey_set(0);
        bsp_system_delay_ms(hold_ms);
        modem_pwrkey_set(1);
    }
    modem_pwrkey_idle_inverted(inverted);
}

/* 老程序语义：PB4 切到有效后保持，不在本阶段再回空闲。 */
static void modem_pwrkey_latch_on_inverted(int inverted, uint32_t hold_ms)
{
    modem_pwrkey_idle_inverted(inverted);
    bsp_system_delay_ms(50U);
    if (inverted) {
        modem_pwrkey_set(1);
    } else {
        modem_pwrkey_set(0);
    }
    bsp_system_delay_ms(hold_ms);
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

    modem_pwrkey_idle_inverted(BOARD_HW_MODEM_PWRKEY_INVERTED);
}

static void stm32f103_afio_release_pb4_from_jtag(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_AFIOEN;
    {
        uint32_t v = AFIO_MAPR;
        v &= ~AFIO_MAPR_SWJ_CFG_MASK;
        v |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SWD_ON;
        AFIO_MAPR = v;
    }
}

/** PWRKEY 空闲 -> 模组电源关 off_ms -> 开 after_on_ms + pre_pwrkey_ms，再调用方发 PWRKEY 脉冲 */
static void modem_vcc_off_on_settle(uint32_t off_ms, uint32_t after_on_ms, uint32_t pre_pwrkey_ms)
{
    modem_pwrkey_idle_inverted(BOARD_HW_MODEM_PWRKEY_INVERTED);
    bsp_debug_log("[4G] ec600n VCC off\r\n");
    modem_power_set(0);
    bsp_system_delay_ms(off_ms);
    bsp_debug_log("[4G] ec600n VCC on\r\n");
    modem_power_set(1);
    bsp_system_delay_ms(after_on_ms);
    bsp_system_delay_ms(pre_pwrkey_ms);
}

#else

static void modem_vcc_off_on_settle(uint32_t off_ms, uint32_t after_on_ms, uint32_t pre_pwrkey_ms)
{
    (void)off_ms;
    (void)after_on_ms;
    (void)pre_pwrkey_ms;
}

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

static void stm32f103_afio_release_pb4_from_jtag(void) {}

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

/** 唤醒失败后：9600/115200 各嗅探一次，区分无 RX、错波特、有响应非 OK；日志仅 ASCII */
static void modem_log_rx_sniff(void)
{
    int      pass;
    int      any = 0;

    for (pass = 0; pass < 2; pass++) {
        uint8_t  tmp[20];
        size_t   n = 0U;
        uint32_t i;

        if (pass == 0) {
            bsp_uart_modem_set_baud(9600U);
        } else {
            bsp_uart_modem_set_baud(115200U);
        }
        modem_drain_hw_rx();
        s_rx_len = 0U;
        for (i = 0U; i < 120U; i++) {
            stream_push_from_uart();
            while (s_rx_len > 0U && n < sizeof(tmp)) {
                tmp[n++] = s_rx_stream[0];
                memmove(s_rx_stream, s_rx_stream + 1U, s_rx_len - 1U);
                s_rx_len--;
            }
            bsp_system_delay_ms(1U);
        }
        if (n == 0U) {
            continue;
        }
        any = 1;
        {
            char  line[100];
            size_t pos = 0U;
            int    w;

            w = snprintf(
                line + pos,
                sizeof(line) - pos,
                "[4G] RX sniff @%s: %u bytes:",
                pass == 0 ? "9600" : "115200",
                (unsigned)n
            );
            if (w > 0) {
                pos += (size_t)w;
            }
            for (i = 0U; i < n && pos + 5U < sizeof(line); i++) {
                w = snprintf(line + pos, sizeof(line) - pos, " %02X", (unsigned)tmp[i]);
                if (w > 0) {
                    pos += (size_t)w;
                }
            }
            (void)snprintf(line + pos, sizeof(line) - pos, "\r\n");
            bsp_debug_log(line);
        }
    }
    if (any == 0) {
        bsp_debug_log("[4G] RX sniff: silent at 9600 and 115200 (check UART wiring)\r\n");
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
    modem_log_at_tx(s);
    size_t len = strlen(s);
    return bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, (const uint8_t *)s, len);
}

static void modem_log_at_tx(const char *cmd)
{
    char line[160];
    size_t len;

    if (cmd == NULL) {
        return;
    }
    len = strcspn(cmd, "\r\n");
    if (len > 96U) {
        len = 96U;
    }
    (void)snprintf(line, sizeof(line), "[4G][AT->MODEM] %.*s\r\n", (int)len, cmd);
    bsp_debug_log(line);
}

static void modem_log_tcp_tx(const uint8_t *data, size_t len)
{
    char line[220];
    uint32_t be_len = 0U;
    size_t json_len;

    if (data == NULL || len == 0U) {
        return;
    }
    if (len >= 4U) {
        be_len = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                 ((uint32_t)data[2] << 8) | (uint32_t)data[3];
        json_len = len - 4U;
        if (json_len > 120U) {
            json_len = 120U;
        }
        (void)snprintf(line, sizeof(line),
                       "[4G][TCP->PLAT] frame_len=%lu prefix=%lu json=%.*s%s\r\n",
                       (unsigned long)len,
                       (unsigned long)be_len,
                       (int)json_len,
                       (const char *)(data + 4U),
                       (len - 4U) > json_len ? "..." : "");
    } else {
        (void)snprintf(line, sizeof(line), "[4G][TCP->PLAT] short frame len=%lu\r\n", (unsigned long)len);
    }
    bsp_debug_log(line);
}

static void modem_log_tcp_rx(const uint8_t *data, size_t len)
{
    char line[220];
    uint32_t be_len = 0U;
    size_t json_len;

    if (data == NULL || len == 0U) {
        return;
    }
    if (len >= 4U) {
        be_len = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                 ((uint32_t)data[2] << 8) | (uint32_t)data[3];
        json_len = len - 4U;
        if (json_len > 120U) {
            json_len = 120U;
        }
        (void)snprintf(line, sizeof(line),
                       "[4G][TCP<-PLAT] frame_len=%lu prefix=%lu json=%.*s%s\r\n",
                       (unsigned long)len,
                       (unsigned long)be_len,
                       (int)json_len,
                       (const char *)(data + 4U),
                       (len - 4U) > json_len ? "..." : "");
    } else {
        (void)snprintf(line, sizeof(line), "[4G][TCP<-PLAT] short frame len=%lu\r\n", (unsigned long)len);
    }
    bsp_debug_log(line);
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
            if (strstr(line, "CONNECT") != NULL) {
                bsp_debug_log("[4G] QIOPEN: CONNECT\r\n");
                return 0;
            }
            if (strstr(line, "+QIOPEN:") != NULL) {
                unsigned cid = 0U;
                int      err = -1;
                if (sscanf(line, "+QIOPEN: %u,%d", &cid, &err) == 2) {
                    if ((int)cid == TCP_CONNECT_ID && err == 0) {
                        return 0;
                    }
                    {
                        char eb[80];
                        (void)snprintf(eb, sizeof(eb), "[4G] QIOPEN err=%d (cid=%u)\r\n", err, cid);
                        bsp_debug_log(eb);
                    }
                    return -1;
                }
                bsp_debug_log("[4G] QIOPEN: bad +QIOPEN line\r\n");
                return -1;
            }
            if (strstr(line, "OK") != NULL) {
                li = 0U;
                continue;
            }
            if (strstr(line, "ERROR") != NULL) {
                bsp_debug_log("[4G] QIOPEN: ERROR before result\r\n");
                return -1;
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }
    bsp_debug_log("[4G] QIOPEN: timeout waiting CONNECT/+QIOPEN\r\n");
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
            modem_log_tcp_rx(tmp, (size_t)chunk);
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
    static uint8_t    s_probe_fail_logs_left = 3U;

    for (int attempt = 0; attempt < 5; ++attempt) {
        modem_drain_hw_rx();
        s_rx_len = 0U;
        (void)bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, (const uint8_t *)at_cmd, sizeof(at_cmd) - 1U);
        if (modem_wait_substr("OK", 1200U) == 0) {
            return 1;
        }
        bsp_system_delay_ms(500U);
    }
    if (s_probe_fail_logs_left > 0U) {
        s_probe_fail_logs_left--;
        bsp_debug_log("[4G] AT probe: no OK after 5 tries (UART/baud/modem?)\r\n");
    }
    return 0;
}

static int modem_at_ping_ok(uint32_t wait_ms)
{
    modem_drain_hw_rx();
    s_rx_len = 0U;
    (void)bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, (const uint8_t *)"AT\r\n", 4U);
    return modem_wait_substr("OK", wait_ms);
}

/** 0=失败；1=115200 已通；2=仅 9600 通（可尝试 AT+IPR 抬升） */
static int modem_wake_at_multibaud(void)
{
    int r;

    bsp_uart_modem_set_baud(115200U);
    for (r = 0; r < 20; r++) {
        if (modem_at_ping_ok(900U) == 0) {
            return 1;
        }
        bsp_system_delay_ms(200U);
    }

    bsp_debug_log("[4G] AT no OK @115200, try 9600...\r\n");
    bsp_uart_modem_set_baud(9600U);
    for (r = 0; r < 20; r++) {
        if (modem_at_ping_ok(900U) == 0) {
            return 2;
        }
        bsp_system_delay_ms(200U);
    }
    return 0;
}

static void modem_try_raise_baud_to_115200(void)
{
    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_at_simple_ok("AT+IPR=115200\r\n", 4000U) != 0) {
        bsp_debug_log("[4G] AT+IPR=115200 failed, stay 9600\r\n");
        return;
    }
    bsp_system_delay_ms(150U);
    bsp_uart_modem_set_baud(115200U);
    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_at_ping_ok(1500U) != 0) {
        bsp_debug_log("[4G] no AT OK after IPR, keep 9600\r\n");
        bsp_uart_modem_set_baud(9600U);
    } else {
        bsp_debug_log("[4G] UART 115200 (AT+IPR)\r\n");
    }
}

static int modem_try_bringup_sequence(void)
{
    int wake;

    bsp_debug_log("[4G] AT wake (115200 then 9600)...\r\n");
    wake = modem_wake_at_multibaud();
    if (wake == 0) {
        modem_log_rx_sniff();
        bsp_debug_log("[4G] no AT OK, skip CFUN (check UART/PWRKEY/VCC)\r\n");
        return 0;
    }
    if (wake == 2) {
        modem_try_raise_baud_to_115200();
    }

    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_at_simple_ok("AT+CFUN=1\r\n", 10000U) != 0) {
        bsp_debug_log("[4G] AT+CFUN=1: no OK\r\n");
    } else {
        bsp_debug_log("[4G] AT+CFUN=1: OK\r\n");
    }
    bsp_system_delay_ms(2000U);
    modem_drain_hw_rx();
    s_rx_len = 0U;

    bsp_debug_log("[4G] AT probe (post-CFUN)...\r\n");
    if (modem_probe_online() != 0) {
        s_online = 1;
        bsp_debug_log("[4G] modem responded to AT\r\n");
        (void)modem_at_simple_ok("ATE0\r\n", 3000U);
        return 1;
    }
    bsp_debug_log("[4G] modem did not respond to AT (poll will retry)\r\n");
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
    /* 先于 UART4：与参考工程一致，避免 NJTRST 占用 PB4 */
    stm32f103_afio_release_pb4_from_jtag();
    bsp_debug_log("[4G] AFIO: JTAG off, PB4=GPIO PWRKEY\r\n");
    bsp_uart_modem_init();
    bsp_uart_modem_log_rcc();
    modem_power_gpio_init();
    modem_pwrkey_gpio_init();
    bsp_uart_modem_reapply_pins();
    bsp_uart_modem_sync_baud_to_rcc(115200U);
    bsp_debug_log("[4G] cold: vcc off -> on + settle (legacy)\r\n");
    modem_vcc_off_on_settle(MODEM_VCC_OFF_MS, MODEM_VCC_ON_SETTLE_MS, MODEM_PRE_PWRKEY_MS);
    modem_drain_hw_rx();

    modem_pwrkey_latch_on_inverted(BOARD_HW_MODEM_PWRKEY_INVERTED, MODEM_PWRKEY_HOLD_MS);
    bsp_debug_log("[4G] PWRKEY latched active (legacy compatible)\r\n");
    if (MODEM_POST_PWRKEY_BOOT_MS > 0U) {
        bsp_system_delay_ms(MODEM_POST_PWRKEY_BOOT_MS);
    }

    if (modem_try_bringup_sequence() != 0) {
        return;
    }

    bsp_debug_log("[4G] retry: opposite PWRKEY + power cycle\r\n");
    modem_vcc_off_on_settle(MODEM_RETRY_VCC_OFF_MS, MODEM_RETRY_VCC_ON_MS, MODEM_RETRY_PRE_KEY_MS);
    modem_drain_hw_rx();
    {
        int alt = !BOARD_HW_MODEM_PWRKEY_INVERTED;
        modem_pwrkey_latch_on_inverted(alt, MODEM_PWRKEY_HOLD_MS);
    }
    if (MODEM_POST_PWRKEY_BOOT_MS > 0U) {
        bsp_system_delay_ms(MODEM_POST_PWRKEY_BOOT_MS);
    }
    if (modem_try_bringup_sequence() != 0) {
        return;
    }
}

static void modem_offline_recovery(uint32_t monotonic_ms)
{
    static uint32_t s_last_recovery_ms;
    static uint8_t  s_offline_notice_once;
    static uint8_t  s_cfun_once;

    if (s_online != 0) {
        return;
    }
    if (monotonic_ms < MODEM_POST_PWRKEY_BOOT_MS) {
        return;
    }
    if (s_last_recovery_ms != 0U && (monotonic_ms - s_last_recovery_ms) < 20000U) {
        return;
    }
    s_last_recovery_ms = monotonic_ms;

    if (s_offline_notice_once == 0U) {
        s_offline_notice_once = 1U;
        bsp_debug_log("[4G] modem offline, retrying AT every ~20s (check power/UART/PWRKEY)\r\n");
    }
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
        bsp_debug_log("[4G] recovery: AT+CFUN=1\r\n");
        if (modem_at_simple_ok("AT+CFUN=1\r\n", 25000U) != 0) {
            bsp_debug_log("[4G] recovery: CFUN no OK\r\n");
        } else {
            bsp_debug_log("[4G] recovery: CFUN OK\r\n");
        }
        bsp_system_delay_ms(3000U);
        modem_drain_hw_rx();
        s_rx_len = 0U;
        if (modem_probe_online() != 0) {
            s_online = 1;
            bsp_debug_log("[4G] modem OK after CFUN (recovery)\r\n");
            (void)modem_at_simple_ok("ATE0\r\n", 3000U);
        } else {
            bsp_debug_log("[4G] recovery: AT still dead after CFUN\r\n");
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
        if (s_online == 0) {
            bsp_debug_log("[4G] TCP skip: modem offline (AT not OK)\r\n");
        } else if (host == NULL || host[0] == '\0') {
            bsp_debug_log("[4G] TCP skip: empty host\r\n");
        } else {
            bsp_debug_log("[4G] TCP skip: port=0\r\n");
        }
        return -1;
    }

    {
        char dial[128];
        size_t hl = strlen(host);
        if (hl > 48U) {
            (void)snprintf(dial, sizeof(dial), "[4G] TCP chain: dial %.48s...:%u\r\n", host, (unsigned)port);
        } else {
            (void)snprintf(dial, sizeof(dial), "[4G] TCP chain: dial %s:%u\r\n", host, (unsigned)port);
        }
        bsp_debug_log(dial);
    }

    modem_drain_hw_rx();
    s_rx_len = 0U;

    {
        char apn_cmd[96];
        char apn_log[96];
        (void)snprintf(apn_log, sizeof(apn_log), "[4G] TCP [1/4] PDP QICSGP APN=%s\r\n", FW_PLATFORM_APN);
        bsp_debug_log(apn_log);
        (void)snprintf(apn_cmd, sizeof(apn_cmd), "AT+QICSGP=1,1,\"%s\",\"\",\"\",1\r\n", FW_PLATFORM_APN);
        if (modem_at_simple_ok(apn_cmd, 5000U) != 0) {
            bsp_debug_log("[4G] TCP [1/4] FAIL: QICSGP (PDP profile)\r\n");
            return -1;
        }
    }
    bsp_debug_log("[4G] TCP [1/4] OK: QICSGP\r\n");

    bsp_debug_log("[4G] TCP [2/4] PDP QIACT (attach, may take 45s)\r\n");
    if (modem_at_simple_ok("AT+QIACT=1\r\n", 45000U) != 0) {
        bsp_debug_log("[4G] TCP [2/4] FAIL: QIACT (SIM/APN/coverage? try AT+CPIN? / AT+CREG? on PC)\r\n");
        return -1;
    }
    bsp_debug_log("[4G] TCP [2/4] OK: QIACT\r\n");

    bsp_debug_log("[4G] TCP [3/4] QICLOSE cleanup\r\n");
    if (modem_at_simple_ok("AT+QICLOSE=0\r\n", 8000U) != 0) {
        bsp_debug_log("[4G] TCP [3/4] WARN: QICLOSE (continuing)\r\n");
    } else {
        bsp_debug_log("[4G] TCP [3/4] OK: QICLOSE\r\n");
    }

    {
        char cmd[180];
        /* 与参考「机井3.0一体机带lora(老曹改后)…network_task.c」EC_QIOPEN 一致：local_port=0, access_mode=2 */
        int  n = snprintf(cmd, sizeof(cmd), "AT+QIOPEN=1,%u,\"TCP\",\"%s\",%u,0,0\r\n", TCP_CONNECT_ID, host, (unsigned)port);
        if (n <= 0 || (size_t)n >= sizeof(cmd)) {
            bsp_debug_log("[4G] TCP [4/4] FAIL: host too long for AT cmd\r\n");
            return -1;
        }
        bsp_debug_log("[4G] TCP [4/4] QIOPEN TCP socket...\r\n");
        modem_drain_hw_rx();
        s_rx_len = 0U;
        if (modem_write_str(cmd) < 0) {
            bsp_debug_log("[4G] TCP [4/4] FAIL: UART write QIOPEN\r\n");
            return -1;
        }
        if (modem_wait_qiopen_result(25000U) != 0) {
            bsp_debug_log("[4G] TCP [4/4] FAIL: QIOPEN (DNS/firewall/server?)\r\n");
            return -1;
        }
    }

    s_tcp_connected = 1;
    bsp_debug_log("[4G] TCP [4/4] OK: socket open, data path ready\r\n");
    bsp_debug_log("[4G] TCP mode: AT control + QISEND/QIRD data plane\r\n");
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
    if (data == NULL || len == 0U || s_tcp_connected == 0) {
        return -1;
    }

    if (len > 1460U) {
        len = 1460U;
    }

    {
        char hdr[48];

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

        modem_log_tcp_tx(data, len);
        if (bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, data, len) != (int)len) {
            return -1;
        }

        if (modem_wait_substr("SEND OK", 15000U) != 0) {
            if (modem_wait_line_ok_or_err(3000U) != 0) {
                bsp_debug_log("[4G] TCP: QISEND failed\r\n");
                return -1;
            }
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
