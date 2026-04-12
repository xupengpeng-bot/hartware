#include "net_4g_modem.h"
#include "fw_build_config.h"
#include "board_hw_config.h"
#include "bsp_system.h"
#include "bsp_rtc.h"
#include "bsp_uart.h"
#include "common_identity.h"
#include "common_status.h"
#include "storage_config.h"

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
#define MODEM_STREAM_CAP   640U
#define MODEM_LINE_MAX     256U
#define TCP_RX_FIFO_CAP    1536U
#define TCP_CONNECT_ID     0U
#define PDP_CONTEXT_ID     1U
#define MODEM_TIME_MIN_YEAR 2024
#define MODEM_TIME_MAX_YEAR 2039
#define MODEM_QIRD_LINE_TIMEOUT_MS 500U
#define MODEM_QIRD_DATA_BYTE_TIMEOUT_MS 200U
#define MODEM_QIRD_TAIL_TIMEOUT_MS 200U
#define MODEM_QIRD_FETCH_MAX 768U
#define MODEM_QIRD_PENDING_RETRY_MS 1000U
#define MODEM_QIRD_PENDING_NO_DATA_LIMIT 8U

static uint8_t  s_rx_stream[MODEM_STREAM_CAP];
static size_t   s_rx_len;
static uint8_t  s_rx_stream_saturated;
static uint8_t  s_tcp_fifo[TCP_RX_FIFO_CAP];
static size_t   s_tcp_head;
static size_t   s_tcp_tail;
static uint32_t s_tcp_fifo_drop_count;
static int      s_online;
static int      s_tcp_connected;
static uint32_t s_last_online_change_ms;
static uint32_t s_last_time_sync_attempt_ms;
static uint32_t s_last_identity_attempt_ms;
static uint32_t s_last_signal_attempt_ms;
static uint8_t  s_qird_fail_streak;
static unsigned s_qird_pending_len;
static uint8_t  s_qird_pending_no_data_streak;
static uint32_t s_qird_pending_retry_after_ms;

typedef struct {
    int      read_len;
    unsigned data_len;
    unsigned unread_len;
} modem_qird_result_t;

#define MODEM_RECOVERY_MAGIC 0x4D524543UL
typedef struct {
    uint32_t magic;
    uint32_t last_recovery_ms;
    uint32_t last_fast_drop_warn_ms;
    uint8_t  offline_notice_once;
    uint8_t  cfun_once;
    uint8_t  reserved[2];
} modem_recovery_state_t;

static modem_recovery_state_t s_recovery;

static void tcp_fifo_push(const uint8_t *p, size_t n);
static void modem_drain_hw_rx(void);
static int modem_getch_ms(uint32_t timeout_ms);
static int modem_write_str(const char *s);
static void modem_log_at_tx(const char *cmd);
static void modem_log_at_rx_line(const char *line);
static void modem_dump_qiact_failure_diagnostics(void);
static void modem_log_tcp_tx(const uint8_t *data, size_t len);
static void modem_log_tcp_rx(const uint8_t *data, size_t len);
static void modem_log_uart_rx_diag(const char *tag);
static void modem_log_qird_header(unsigned request_len, const char *raw_line, int a, int b, int c, int fields);
static void modem_log_qird_result(const char *tag, unsigned request_len, int read_len, unsigned data_len, unsigned unread_len);
static void modem_set_online_state(int online, const char *reason, uint32_t monotonic_ms);
static void modem_recovery_state_reset(void);
static void modem_recovery_state_validate_or_reset(uint32_t monotonic_ms);
static int modem_try_refresh_identity(void);
static int modem_query_cgatt_attached(void);
static int modem_try_refresh_signal(void);
static void modem_reset_qird_pending_state(void);

static void modem_reset_qird_pending_state(void)
{
    s_qird_fail_streak = 0U;
    s_qird_pending_len = 0U;
    s_qird_pending_no_data_streak = 0U;
    s_qird_pending_retry_after_ms = 0U;
}

static int modem_extract_span_token(const char *line, char *out, size_t out_cap,
                                    size_t min_len, size_t max_len, int allow_alpha)
{
    size_t i = 0U;
    size_t best_start = 0U;
    size_t best_len = 0U;
    int found = 0;

    if (line == NULL || out == NULL || out_cap == 0U || min_len == 0U) {
        return -1;
    }

    while (line[i] != '\0') {
        size_t start = i;
        size_t len = 0U;
        while ((line[i] >= '0' && line[i] <= '9') ||
               (allow_alpha != 0 &&
                ((line[i] >= 'A' && line[i] <= 'Z') ||
                 (line[i] >= 'a' && line[i] <= 'z')))) {
            i++;
            len++;
        }
        if (len >= min_len) {
            if (max_len == 0U || len <= max_len) {
                best_start = start;
                best_len = len;
                found = 1;
                break;
            }
            best_start = start;
            best_len = max_len;
            found = 1;
            break;
        }
        if (len == 0U) {
            i++;
        }
    }

    if (found == 0) {
        return -1;
    }
    if (best_len >= out_cap) {
        best_len = out_cap - 1U;
    }
    memcpy(out, line + best_start, best_len);
    out[best_len] = '\0';
    return (int)best_len;
}

static int modem_identity_is_ready(void)
{
    const controller_identity_t *id = common_identity_get();

    if (id == NULL) {
        return 0;
    }
    return (id->imei[0] != '\0' && id->iccid[0] != '\0') ? 1 : 0;
}

static int modem_query_identity_value(const char *cmd, char *out, size_t out_cap,
                                      size_t min_len, size_t max_len, int allow_alpha)
{
    char line[MODEM_LINE_MAX];
    size_t li = 0U;
    uint32_t waited = 0U;
    int saw_value = 0;

    if (cmd == NULL || out == NULL || out_cap == 0U) {
        return -1;
    }

    modem_drain_hw_rx();
    s_rx_len = 0U;
    out[0] = '\0';
    if (modem_write_str(cmd) < 0) {
        return -1;
    }

    while (waited < 5000U) {
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
            if (li > 0U) {
                modem_log_at_rx_line(line);
                if (modem_extract_span_token(line, out, out_cap, min_len, max_len, allow_alpha) > 0) {
                    saw_value = 1;
                }
                if (strstr(line, "OK") != NULL) {
                    return saw_value != 0 ? 0 : -1;
                }
                if (strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) {
                    return -1;
                }
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }

    return saw_value != 0 ? 0 : -1;
}

static int modem_query_cgatt_attached(void)
{
    char line[MODEM_LINE_MAX];
    size_t li = 0U;
    uint32_t waited = 0U;
    int attached = 0;

    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_write_str("AT+CGATT?\r\n") < 0) {
        return -1;
    }

    while (waited < 3000U) {
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
            if (li > 0U) {
                modem_log_at_rx_line(line);
                if (strstr(line, "+CGATT: 1") != NULL) {
                    attached = 1;
                }
                if (strstr(line, "OK") != NULL) {
                    return attached;
                }
                if (strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) {
                    return -1;
                }
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }
    return attached;
}

static int modem_try_refresh_signal(void)
{
    char line[MODEM_LINE_MAX];
    size_t li = 0U;
    uint32_t waited = 0U;
    int csq = -1;

    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_write_str("AT+CSQ\r\n") < 0) {
        return -1;
    }

    while (waited < 3000U) {
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
            if (li > 0U) {
                modem_log_at_rx_line(line);
                if (sscanf(line, "+CSQ: %d", &csq) == 1) {
                    common_status_set_signal((int16_t)csq, 0, 0);
                }
                if (strstr(line, "OK") != NULL) {
                    return csq >= 0 ? 0 : -1;
                }
                if (strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) {
                    return -1;
                }
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }

    return csq >= 0 ? 0 : -1;
}

static int modem_try_refresh_identity(void)
{
    controller_identity_t *id = common_identity_mutable();
    char imei[CTRL_IMEI_LEN];
    char iccid[CTRL_ICCID_LEN];
    char line[160];
    int refreshed = 0;

    if (id == NULL || s_online == 0) {
        return -1;
    }

    if (id->imei[0] == '\0') {
        imei[0] = '\0';
        if (modem_query_identity_value("AT+CGSN\r\n", imei, sizeof(imei), 14U, 20U, 0) == 0) {
            (void)strncpy(id->imei, imei, sizeof(id->imei) - 1U);
            (void)snprintf(line, sizeof(line), "[4G] modem identity imei=%s\r\n", id->imei);
            bsp_debug_log(line);
            refreshed = 1;
        } else {
            bsp_debug_log("[4G] modem identity IMEI read failed\r\n");
        }
    }

    if (id->iccid[0] == '\0') {
        iccid[0] = '\0';
        if (modem_query_identity_value("AT+QCCID\r\n", iccid, sizeof(iccid), 18U, 24U, 1) == 0) {
            (void)strncpy(id->iccid, iccid, sizeof(id->iccid) - 1U);
            (void)snprintf(line, sizeof(line), "[4G] modem identity iccid=%s\r\n", id->iccid);
            bsp_debug_log(line);
            refreshed = 1;
        } else {
            bsp_debug_log("[4G] modem identity ICCID read failed\r\n");
        }
    }

    return modem_identity_is_ready() != 0 ? 0 : (refreshed != 0 ? -2 : -1);
}

static int modem_timezone_qh_valid(int tzq)
{
    return (tzq >= -48 && tzq <= 56) ? 1 : 0;
}

static int modem_time_fields_plausible(int year, int mo, int dd, int hh, int mm, int ss)
{
    if (year < MODEM_TIME_MIN_YEAR || year > MODEM_TIME_MAX_YEAR) {
        return 0;
    }
    if (mo < 1 || mo > 12 || dd < 1 || dd > 31) {
        return 0;
    }
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
        return 0;
    }
    return 1;
}

static int modem_configured_timezone_qh(void)
{
    const device_config_t *cfg = storage_config_active();

    if (cfg != NULL && modem_timezone_qh_valid(cfg->time_zone_quarter_hours) != 0) {
        return cfg->time_zone_quarter_hours;
    }
    return MODEL_DEFAULT_TIME_ZONE_QUARTER_HOURS;
}

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
    s_rx_stream_saturated = 0U;
}

static void stream_push_from_uart(void)
{
    uint8_t ch;
    while (bsp_uart_read((int)BOARD_HW_UART_PORT_MODEM_4G, &ch, 1U) > 0) {
        if (s_rx_len < MODEM_STREAM_CAP) {
            s_rx_stream[s_rx_len++] = ch;
        } else {
            if (s_rx_stream_saturated == 0U) {
                char line[120];
                (void)snprintf(line, sizeof(line),
                               "[4G] RX stream full at %u bytes, defer further UART reads to preserve unread data\r\n",
                               (unsigned)MODEM_STREAM_CAP);
                bsp_debug_log(line);
                s_rx_stream_saturated = 1U;
            }
            break;
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
            if (s_rx_stream_saturated != 0U && s_rx_len < MODEM_STREAM_CAP) {
                s_rx_stream_saturated = 0U;
            }
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
            s_tcp_fifo_drop_count++;
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
    if (s_tcp_fifo_drop_count > 0U) {
        char line[112];
        (void)snprintf(line, sizeof(line),
                       "[4G] TCP RX fifo overwrite drop_bytes=%lu cap=%u\r\n",
                       (unsigned long)s_tcp_fifo_drop_count,
                       (unsigned)TCP_RX_FIFO_CAP);
        bsp_debug_log(line);
        s_tcp_fifo_drop_count = 0U;
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
    (void)snprintf(line, sizeof(line), "[AT-TX] %.*s\r\n", (int)len, cmd);
    bsp_debug_log(line);
}

static void modem_log_at_rx_line(const char *line)
{
    char buf[160];
    size_t len;

    if (line == NULL || line[0] == '\0') {
        return;
    }
    len = strlen(line);
    if (len > 96U) {
        len = 96U;
    }
    (void)snprintf(buf, sizeof(buf), "[AT-RX] %.*s\r\n", (int)len, line);
    bsp_debug_log(buf);
}

static void modem_run_diag_command(const char *label, const char *cmd, uint32_t timeout_ms)
{
    char     line[MODEM_LINE_MAX];
    size_t   li = 0U;
    uint32_t waited = 0U;
    int      finished = 0;

    if (label == NULL || cmd == NULL) {
        return;
    }

    bsp_debug_log("[4G][DIAG] ");
    bsp_debug_log(label);
    bsp_debug_log("\r\n");

    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_write_str(cmd) < 0) {
        bsp_debug_log("[4G][DIAG] UART write failed\r\n");
        return;
    }

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
            if (li > 0U) {
                modem_log_at_rx_line(line);
                if (strstr(line, "OK") != NULL || strstr(line, "ERROR") != NULL ||
                    strstr(line, "+CME ERROR") != NULL) {
                    finished = 1;
                    break;
                }
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }

    if (!finished) {
        bsp_debug_log("[4G][DIAG] timeout\r\n");
    }
}

static void modem_dump_qiact_failure_diagnostics(void)
{
    bsp_debug_log("[4G][DIAG] QIACT failure snapshot begin\r\n");
    modem_run_diag_command("AT+CPIN?", "AT+CPIN?\r\n", 4000U);
    modem_run_diag_command("AT+CSQ", "AT+CSQ\r\n", 4000U);
    modem_run_diag_command("AT+CREG?", "AT+CREG?\r\n", 4000U);
    modem_run_diag_command("AT+CGATT?", "AT+CGATT?\r\n", 4000U);
    modem_run_diag_command("AT+QIACT?", "AT+QIACT?\r\n", 5000U);
    bsp_debug_log("[4G][DIAG] QIACT failure snapshot end\r\n");
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
                       "[TCP-TX] frame_len=%lu prefix=%lu json=%.*s%s\r\n",
                       (unsigned long)len,
                       (unsigned long)be_len,
                       (int)json_len,
                       (const char *)(data + 4U),
                       (len - 4U) > json_len ? "..." : "");
    } else {
        (void)snprintf(line, sizeof(line), "[TCP-TX] short frame len=%lu\r\n", (unsigned long)len);
    }
    bsp_debug_log(line);
}

static void modem_hex_window(const uint8_t *data, size_t len, char *out, size_t out_cap)
{
    size_t pos = 0U;
    size_t i;
    size_t head_len;
    size_t tail_len;

    if (out == NULL || out_cap == 0U) {
        return;
    }
    out[0] = '\0';
    if (data == NULL || len == 0U) {
        return;
    }

    head_len = len > 8U ? 8U : len;
    for (i = 0U; i < head_len && pos + 4U < out_cap; i++) {
        int wrote = snprintf(out + pos, out_cap - pos, "%02X%s",
                             (unsigned)data[i],
                             (i + 1U < head_len) ? " " : "");
        if (wrote <= 0) {
            return;
        }
        pos += (size_t)wrote;
    }

    if (len > head_len && pos + 6U < out_cap) {
        int wrote = snprintf(out + pos, out_cap - pos, " .. ");
        if (wrote <= 0) {
            return;
        }
        pos += (size_t)wrote;
    }

    tail_len = len > 8U ? 8U : 0U;
    for (i = (tail_len > 0U) ? (len - tail_len) : len; i < len && pos + 4U < out_cap; i++) {
        int wrote = snprintf(out + pos, out_cap - pos, "%02X%s",
                             (unsigned)data[i],
                             (i + 1U < len) ? " " : "");
        if (wrote <= 0) {
            return;
        }
        pos += (size_t)wrote;
    }
}

static void modem_log_tcp_rx(const uint8_t *data, size_t len)
{
    char line[320];
    char hex[96];
    uint32_t be_len = 0U;
    size_t json_len;

    if (data == NULL || len == 0U) {
        return;
    }
    modem_hex_window(data, len, hex, sizeof(hex));
    if (len >= 4U) {
        be_len = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                 ((uint32_t)data[2] << 8) | (uint32_t)data[3];
        if ((size_t)be_len + 4U > len) {
            size_t shown = len > 120U ? 120U : len;
            (void)snprintf(line, sizeof(line),
                           "[TCP-RX] chunk_len=%lu partial_prefix=%lu bytes=%.*s%s hex=%s\r\n",
                           (unsigned long)len,
                           (unsigned long)be_len,
                           (int)shown,
                           (const char *)data,
                           len > shown ? "..." : "",
                           hex);
            bsp_debug_log(line);
            return;
        }
        json_len = len - 4U;
        if (json_len > 120U) {
            json_len = 120U;
        }
        (void)snprintf(line, sizeof(line),
                       "[TCP-RX] frame_len=%lu prefix=%lu json=%.*s%s hex=%s\r\n",
                       (unsigned long)len,
                       (unsigned long)be_len,
                       (int)json_len,
                       (const char *)(data + 4U),
                       (len - 4U) > json_len ? "..." : "",
                       hex);
    } else {
        (void)snprintf(line, sizeof(line), "[TCP-RX] short frame len=%lu hex=%s\r\n",
                       (unsigned long)len, hex);
    }
    bsp_debug_log(line);
}

static void modem_log_uart_rx_diag(const char *tag)
{
    uint32_t fifo_drop = 0U;
    uint32_t ore = 0U;
    uint32_t fe = 0U;
    uint32_t ne = 0U;
    uint32_t pe = 0U;

    bsp_uart_modem_take_rx_diag(&fifo_drop, &ore, &fe, &ne, &pe);
    if (fifo_drop == 0U && ore == 0U && fe == 0U && ne == 0U && pe == 0U) {
        return;
    }

    {
        char line[176];
        (void)snprintf(line, sizeof(line),
                       "[4G] UART4 rx diag tag=%s fifo_drop=%lu ore=%lu fe=%lu ne=%lu pe=%lu\r\n",
                       tag != NULL ? tag : "unknown",
                       (unsigned long)fifo_drop,
                       (unsigned long)ore,
                       (unsigned long)fe,
                       (unsigned long)ne,
                       (unsigned long)pe);
        bsp_debug_log(line);
    }
}

static void modem_log_qird_header(unsigned request_len, const char *raw_line, int a, int b, int c, int fields)
{
    char line[224];
    size_t raw_len = 0U;

    if (raw_line != NULL) {
        raw_len = strlen(raw_line);
        if (raw_len > 96U) {
            raw_len = 96U;
        }
    }
    (void)snprintf(line, sizeof(line),
                   "[4G] QIRD header req=%u fields=%d parsed=%d,%d,%d raw=%.*s\r\n",
                   request_len,
                   fields,
                   a,
                   b,
                   c,
                   (int)raw_len,
                   raw_line != NULL ? raw_line : "");
    bsp_debug_log(line);
}

static void modem_log_qird_result(const char *tag, unsigned request_len, int read_len, unsigned data_len, unsigned unread_len)
{
    char line[176];
    unsigned pending_hint = 0U;

    if (data_len > (unsigned)read_len) {
        pending_hint = (data_len - (unsigned)read_len) + unread_len;
    } else {
        pending_hint = unread_len;
    }
    (void)snprintf(line, sizeof(line),
                   "[4G] QIRD result tag=%s req=%u read=%d data=%u unread=%u pending_hint=%u\r\n",
                   tag != NULL ? tag : "unknown",
                   request_len,
                   read_len,
                   data_len,
                   unread_len,
                   pending_hint);
    bsp_debug_log(line);
}

static void modem_set_online_state(int online, const char *reason, uint32_t monotonic_ms)
{
    char line[160];

    if (s_online == online) {
        return;
    }
    s_online = online;
    s_last_online_change_ms = monotonic_ms;
    (void)snprintf(line, sizeof(line),
                   "[4G] state: online=%d @%lu ms (%s)\r\n",
                   online != 0 ? 1 : 0,
                   (unsigned long)monotonic_ms,
                   (reason != NULL) ? reason : "no reason");
    bsp_debug_log(line);
}

static void modem_recovery_state_reset(void)
{
    memset(&s_recovery, 0, sizeof(s_recovery));
    s_recovery.magic = MODEM_RECOVERY_MAGIC;
}

static void modem_recovery_state_validate_or_reset(uint32_t monotonic_ms)
{
    if (s_recovery.magic != MODEM_RECOVERY_MAGIC) {
        char line[160];
        (void)snprintf(line, sizeof(line),
                       "[4G] WARN: recovery state magic corrupted (0x%08lX), reset state @%lu ms\r\n",
                       (unsigned long)s_recovery.magic,
                       (unsigned long)monotonic_ms);
        bsp_debug_log(line);
        modem_recovery_state_reset();
    }
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

static void modem_copy_line(char *dst, size_t dst_cap, const char *src)
{
    size_t n;

    if (dst == NULL || dst_cap == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= dst_cap) {
        n = dst_cap - 1U;
    }
    if (n > 0U) {
        memcpy(dst, src, n);
    }
    dst[n] = '\0';
}

static int modem_wait_line_ok_or_err_capture(uint32_t timeout_ms, char *last_line, size_t last_line_cap)
{
    char     line[MODEM_LINE_MAX];
    size_t   li = 0U;
    uint32_t waited = 0U;

    if (last_line != NULL && last_line_cap > 0U) {
        last_line[0] = '\0';
    }

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
            if (li > 0U) {
                modem_copy_line(last_line, last_line_cap, line);
            }
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

static int modem_wait_line_ok_or_err(uint32_t timeout_ms)
{
    return modem_wait_line_ok_or_err_capture(timeout_ms, NULL, 0U);
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

static int modem_at_simple_ok_capture(const char *cmd, uint32_t timeout_ms, char *last_line, size_t last_line_cap)
{
    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (last_line != NULL && last_line_cap > 0U) {
        last_line[0] = '\0';
    }
    if (modem_write_str(cmd) < 0) {
        modem_copy_line(last_line, last_line_cap, "UART write failed");
        return -1;
    }
    return modem_wait_line_ok_or_err_capture(timeout_ms, last_line, last_line_cap);
}

static int modem_parse_qiact_active_line(const char *line, unsigned target_cid)
{
    unsigned cid = 0U;
    unsigned state = 0U;
    unsigned type = 0U;

    if (line == NULL) {
        return 0;
    }

    if (sscanf(line, "+QIACT: %u,%u,%u", &cid, &state, &type) >= 2) {
        return (cid == target_cid && state == 1U) ? 1 : 0;
    }

    return 0;
}

static int modem_query_qiact_active(unsigned target_cid, char *matched_line, size_t matched_line_cap)
{
    char     line[MODEM_LINE_MAX];
    size_t   li = 0U;
    uint32_t waited = 0U;
    int      saw_active = 0;

    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (matched_line != NULL && matched_line_cap > 0U) {
        matched_line[0] = '\0';
    }
    if (modem_write_str("AT+QIACT?\r\n") < 0) {
        return -1;
    }

    while (waited < 5000U) {
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
            if (li > 0U) {
                modem_log_at_rx_line(line);
                if (modem_parse_qiact_active_line(line, target_cid) != 0) {
                    saw_active = 1;
                    modem_copy_line(matched_line, matched_line_cap, line);
                }
                if (strstr(line, "OK") != NULL) {
                    return saw_active;
                }
                if (strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) {
                    return saw_active != 0 ? 1 : -1;
                }
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }

    return saw_active;
}

static int64_t days_from_civil_ymd(int year, unsigned month, unsigned day)
{
    year -= (month <= 2U) ? 1 : 0;
    {
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = (unsigned)(year - era * 400);
        const unsigned doy = (153U * (month + (month > 2U ? (unsigned)-3 : 9U)) + 2U) / 5U + day - 1U;
        const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
        return (int64_t)era * 146097LL + (int64_t)doe - 719468LL;
    }
}

static int modem_parse_time_fields_to_unix(int year, int mo, int dd, int hh, int mm, int ss,
                                           char sign, int tzq, uint32_t *out_unix,
                                           int *out_tzq, int *out_used_fallback)
{
    int used_fallback = 0;
    int64_t days;
    int64_t unix_sec;

    if (out_unix == NULL) {
        return -1;
    }
    if (modem_time_fields_plausible(year, mo, dd, hh, mm, ss) == 0) {
        return -2;
    }
    if ((sign != '+' && sign != '-') || modem_timezone_qh_valid(tzq) == 0) {
        tzq = modem_configured_timezone_qh();
        sign = '+';
        if (tzq < 0) {
            sign = '-';
            tzq = -tzq;
        }
        used_fallback = 1;
    }
    days = days_from_civil_ymd(year, (unsigned)mo, (unsigned)dd);
    unix_sec = days * 86400LL + (int64_t)hh * 3600LL + (int64_t)mm * 60LL + (int64_t)ss;
    if (sign == '+') {
        unix_sec -= (int64_t)tzq * 15LL * 60LL;
    } else {
        unix_sec += (int64_t)tzq * 15LL * 60LL;
    }
    if (unix_sec <= 0LL || unix_sec > 0xFFFFFFFFLL) {
        return -1;
    }
    *out_unix = (uint32_t)unix_sec;
    if (out_tzq != NULL) {
        *out_tzq = (sign == '-') ? -tzq : tzq;
    }
    if (out_used_fallback != NULL) {
        *out_used_fallback = used_fallback;
    }
    return 0;
}

static int modem_parse_cclk_to_unix(const char *line, uint32_t *out_unix, int *out_tzq, int *out_used_fallback)
{
    int yy = 0, mo = 0, dd = 0, hh = 0, mm = 0, ss = 0, tzq = 0;
    char sign = '+';
    int parsed = 0;

    if (line == NULL || out_unix == NULL) {
        return -1;
    }
    parsed = sscanf(line, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d%c%2d\"", &yy, &mo, &dd, &hh, &mm, &ss, &sign, &tzq);
    if (parsed < 6) {
        return -1;
    }
    yy += (yy >= 70) ? 1900 : 2000;
    if (parsed < 8) {
        sign = '+';
        tzq = modem_configured_timezone_qh();
    }
    return modem_parse_time_fields_to_unix(yy, mo, dd, hh, mm, ss,
                                           sign, tzq, out_unix, out_tzq, out_used_fallback);
}

static int modem_parse_qlts_to_unix(const char *line, uint32_t *out_unix, int *out_tzq, int *out_used_fallback)
{
    int year = 0, mo = 0, dd = 0, hh = 0, mm = 0, ss = 0, tzq = 0;
    char sign = '+';
    int parsed = 0;

    if (line == NULL || out_unix == NULL) {
        return -1;
    }
    parsed = sscanf(line, "+QLTS: \"%4d/%2d/%2d,%2d:%2d:%2d%c%2d,%*d\"",
                    &year, &mo, &dd, &hh, &mm, &ss, &sign, &tzq);
    if (parsed < 6) {
        return -1;
    }
    if (parsed < 8) {
        sign = '+';
        tzq = modem_configured_timezone_qh();
    }
    return modem_parse_time_fields_to_unix(year, mo, dd, hh, mm, ss,
                                           sign, tzq, out_unix, out_tzq, out_used_fallback);
}

static int modem_try_sync_time_query(const char *cmd, const char *source_tag,
                                     int (*parser)(const char *, uint32_t *, int *, int *))
{
    char line[MODEM_LINE_MAX];
    size_t li = 0U;
    uint32_t unix_sec = 0U;
    uint32_t waited = 0U;
    int tzq = 0;
    int used_fallback = 0;
    int saw_synced = 0;

    if (cmd == NULL || source_tag == NULL || parser == NULL) {
        return -1;
    }

    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_write_str(cmd) < 0) {
        return -1;
    }

    while (waited < 5000U) {
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
            if (li > 0U) {
                {
                    char rx[160];
                    (void)snprintf(rx, sizeof(rx), "[AT-RX] %.148s\r\n", line);
                    bsp_debug_log(rx);
                }
                if (parser(line, &unix_sec, &tzq, &used_fallback) == 0) {
                    if (used_fallback != 0) {
                        char tzlog[80];
                        (void)snprintf(tzlog, sizeof(tzlog),
                                       "[TIME] %s timezone fallback qh=%d\r\n", source_tag, tzq);
                        bsp_debug_log(tzlog);
                    }
                    if (bsp_rtc_set_unix(unix_sec) == 0) {
                        char ok[160];
                        char local_ts[32];
                        if (bsp_rtc_format_iso8601(local_ts, sizeof(local_ts), unix_sec, tzq) == 0) {
                            (void)snprintf(ok, sizeof(ok),
                                           "[TIME] network time synced src=%s unix=%lu local=%s tzq=%d\r\n",
                                           source_tag, (unsigned long)unix_sec, local_ts, tzq);
                        } else {
                            (void)snprintf(ok, sizeof(ok),
                                           "[TIME] network time synced src=%s unix=%lu tzq=%d\r\n",
                                           source_tag, (unsigned long)unix_sec, tzq);
                        }
                        bsp_debug_log(ok);
                        saw_synced = 1;
                    }
                } else if (strncmp(line, "+QLTS:", 6) == 0 || strncmp(line, "+CCLK:", 6) == 0) {
                    char bad[160];
                    size_t shown = strlen(line);
                    if (shown > 72U) {
                        shown = 72U;
                    }
                    (void)snprintf(bad, sizeof(bad),
                                   "[TIME] ignore implausible modem clock src=%s raw=%.*s%s\r\n",
                                   source_tag,
                                   (int)shown,
                                   line,
                                   strlen(line) > shown ? "..." : "");
                    bsp_debug_log(bad);
                } else if (strstr(line, "+QLTS: \"\"") != NULL) {
                    bsp_debug_log("[TIME] QLTS local time not available yet\r\n");
                    return -1;
                } else if (strstr(line, "OK") != NULL) {
                    if (saw_synced != 0) {
                        return 0;
                    }
                    if (bsp_rtc_is_synced() == 0) {
                        bsp_debug_log("[TIME] network time not available yet\r\n");
                    }
                    return -1;
                } else if (strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) {
                    char err[96];
                    (void)snprintf(err, sizeof(err), "[TIME] %s query failed\r\n", source_tag);
                    bsp_debug_log(err);
                    return -1;
                }
            }
            li = 0U;
            continue;
        }
        if (li + 1U < sizeof(line)) {
            line[li++] = (char)ch;
        }
    }
    if (bsp_rtc_is_synced() == 0) {
        bsp_debug_log("[TIME] sync timeout\r\n");
    }
    return bsp_rtc_is_synced() ? 0 : -1;
}

static int modem_try_sync_time(void)
{
    if (modem_try_sync_time_query("AT+QLTS=2\r\n", "QLTS", modem_parse_qlts_to_unix) == 0) {
        return 0;
    }
    return modem_try_sync_time_query("AT+CCLK?\r\n", "CCLK", modem_parse_cclk_to_unix);
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

static int modem_qird_fetch(unsigned request_len, modem_qird_result_t *result)
{
    char    cmd[48];
    char    line[MODEM_LINE_MAX];
    int     n_data = -1;
    unsigned unread_len = 0U;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }

    if (request_len == 0U) {
        return 0;
    }
    /* 单次读取与 TCP 缓存匹配，剩余数据由下一次 +QIURC 再拉取 */
    if (request_len > MODEM_QIRD_FETCH_MAX) {
        request_len = MODEM_QIRD_FETCH_MAX;
    }
    {
        char logline[96];
        (void)snprintf(logline, sizeof(logline), "[4G] QIRD fetch request_len=%u\r\n", request_len);
        bsp_debug_log(logline);
    }
    modem_log_uart_rx_diag("pre_qird");
    (void)snprintf(cmd, sizeof(cmd), "AT+QIRD=%u,%u\r\n", TCP_CONNECT_ID, (unsigned)request_len);
    if (modem_write_str(cmd) < 0) {
        return -1;
    }

    for (;;) {
        size_t li = 0U;
        for (;;) {
            int ch = modem_getch_ms(MODEM_QIRD_LINE_TIMEOUT_MS);
            if (ch < 0) {
                bsp_debug_log("[4G] QIRD timeout waiting header line\r\n");
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
                modem_log_qird_header(request_len, line, a, b, c, 3);
                n_data = b;
                unread_len = (c > 0) ? (unsigned)c : 0U;
            } else if (sscanf(line, "+QIRD: %d", &a) == 1) {
                modem_log_qird_header(request_len, line, a, 0, 0, 1);
                n_data = a;
            } else {
                modem_log_qird_header(request_len, line, a, b, c, 0);
                return -1;
            }
            break;
        }
        if (strstr(line, "ERROR") != NULL) {
            return -1;
        }
    }

    if (n_data <= 0) {
        if (result != NULL) {
            result->read_len = 0;
            result->data_len = 0U;
            result->unread_len = unread_len;
        }
        modem_log_qird_result("empty", request_len, 0, 0U, unread_len);
        for (;;) {
            int ch = modem_getch_ms(MODEM_QIRD_TAIL_TIMEOUT_MS);
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
        int     total_read = 0;
        while (remain > 0) {
            int chunk = remain > (int)sizeof(tmp) ? (int)sizeof(tmp) : remain;
            int read_in_chunk = 0;
            while (read_in_chunk < chunk) {
                int ch = modem_getch_ms(MODEM_QIRD_DATA_BYTE_TIMEOUT_MS);
                if (ch < 0) {
                    if (read_in_chunk > 0) {
                        tcp_fifo_push(tmp, (size_t)read_in_chunk);
                        modem_log_tcp_rx(tmp, (size_t)read_in_chunk);
                        total_read += read_in_chunk;
                    }
                    if (total_read > 0) {
                        char part[96];
                        if (result != NULL) {
                            result->read_len = total_read;
                            result->data_len = (unsigned)n_data;
                            result->unread_len = unread_len;
                        }
                        (void)snprintf(part, sizeof(part),
                                       "[4G] QIRD partial payload read=%d/%d, keep TCP alive\r\n",
                                       total_read, n_data);
                        bsp_debug_log(part);
                        modem_log_qird_result("partial", request_len, total_read, (unsigned)n_data, unread_len);
                        modem_log_uart_rx_diag("partial_qird");
                        return total_read;
                    }
                    bsp_debug_log("[4G] QIRD timeout waiting payload byte\r\n");
                    modem_log_uart_rx_diag("qird_timeout");
                    return -1;
                }
                tmp[read_in_chunk++] = (uint8_t)ch;
            }
            tcp_fifo_push(tmp, (size_t)chunk);
            modem_log_tcp_rx(tmp, (size_t)chunk);
            total_read += chunk;
            remain -= chunk;
        }
    }

    (void)modem_wait_line_ok_or_err(MODEM_QIRD_TAIL_TIMEOUT_MS);
    modem_log_uart_rx_diag("post_qird");
    if (result != NULL) {
        result->read_len = n_data;
        result->data_len = (unsigned)n_data;
        result->unread_len = unread_len;
    }
    modem_log_qird_result("complete", request_len, n_data, (unsigned)n_data, unread_len);
    return n_data;
}

static void handle_urc_line(const char *line)
{
    if (line == NULL) {
        return;
    }
    if (strstr(line, "+QIURC: \"closed\"") != NULL) {
        modem_reset_qird_pending_state();
        s_tcp_connected = 0;
        bsp_debug_log("[4G] TCP closed by peer\r\n");
        return;
    }
    if (strstr(line, "+QIURC: \"recv\"") == NULL) {
        return;
    }
    bsp_debug_log("[4G] TCP recv URC\r\n");

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
                    if (recv_len == 0U || recv_len > MODEM_QIRD_FETCH_MAX) {
                        recv_len = MODEM_QIRD_FETCH_MAX;
                    }
                }
            }
        }
    }
    {
        char linebuf[112];
        (void)snprintf(linebuf, sizeof(linebuf),
                       "[4G] TCP recv URC announced=%u pending_before=%u\r\n",
                       recv_len,
                       s_qird_pending_len);
        bsp_debug_log(linebuf);
    }
    {
        modem_qird_result_t qird;
        int fetch_rc = modem_qird_fetch(recv_len, &qird);
        if (fetch_rc < 0) {
            s_qird_fail_streak++;
            if (s_qird_fail_streak >= 3U) {
                modem_reset_qird_pending_state();
                s_tcp_connected = 0;
                bsp_debug_log("[4G] QIRD fetch failed repeatedly, mark TCP disconnected\r\n");
            } else {
                char linebuf[96];
                (void)snprintf(linebuf, sizeof(linebuf),
                               "[4G] QIRD fetch failed streak=%u, keep TCP alive\r\n",
                               (unsigned)s_qird_fail_streak);
                bsp_debug_log(linebuf);
            }
            return;
        }
        s_qird_fail_streak = 0U;
        s_qird_pending_no_data_streak = 0U;
        s_qird_pending_retry_after_ms = 0U;
        if (qird.data_len > (unsigned)qird.read_len) {
            s_qird_pending_len = (qird.data_len - (unsigned)qird.read_len) + qird.unread_len;
        } else {
            s_qird_pending_len = qird.unread_len;
        }
        if (s_qird_pending_len > 0U) {
            {
                char linebuf[112];
                (void)snprintf(linebuf, sizeof(linebuf),
                               "[4G] QIRD pending remain=%u after fetch=%d/%u unread=%u\r\n",
                               s_qird_pending_len,
                               qird.read_len,
                               qird.data_len,
                               qird.unread_len);
                bsp_debug_log(linebuf);
            }
        }
    }
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
        if (s_rx_stream_saturated != 0U && s_rx_len < MODEM_STREAM_CAP) {
            s_rx_stream_saturated = 0U;
        }

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
        modem_set_online_state(1, "bringup probe after CFUN", 0U);
        bsp_debug_log("[4G] modem responded to AT\r\n");
        (void)modem_at_simple_ok("ATE0\r\n", 3000U);
        (void)modem_try_refresh_identity();
        (void)modem_try_sync_time();
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
    s_rx_stream_saturated = 0U;
    s_tcp_head = 0U;
    s_tcp_tail = 0U;
    s_last_online_change_ms = 0U;
    s_last_time_sync_attempt_ms = 0U;
    s_last_identity_attempt_ms = 0U;
    s_last_signal_attempt_ms = 0U;
    modem_reset_qird_pending_state();
    modem_recovery_state_reset();

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
    modem_recovery_state_validate_or_reset(monotonic_ms);

    if (s_online != 0) {
        return;
    }
#if MODEM_POST_PWRKEY_BOOT_MS > 0U
    if (monotonic_ms < MODEM_POST_PWRKEY_BOOT_MS) {
        return;
    }
#endif
    if (s_recovery.last_recovery_ms != 0U &&
        (monotonic_ms - s_recovery.last_recovery_ms) < 20000U) {
        if ((uint32_t)(monotonic_ms - s_last_online_change_ms) < 2000U &&
            (s_recovery.last_fast_drop_warn_ms == 0U ||
             (uint32_t)(monotonic_ms - s_recovery.last_fast_drop_warn_ms) >= 5000U)) {
            char line[168];
            s_recovery.last_fast_drop_warn_ms = monotonic_ms;
            (void)snprintf(line, sizeof(line),
                           "[4G] WARN: online flag dropped again only %lu ms after last state change; recovery suppressed until 20s window\r\n",
                           (unsigned long)(monotonic_ms - s_last_online_change_ms));
            bsp_debug_log(line);
        }
        return;
    }
    s_recovery.last_recovery_ms = monotonic_ms;

    if (s_recovery.offline_notice_once == 0U) {
        s_recovery.offline_notice_once = 1U;
        bsp_debug_log("[4G] modem offline, retrying AT every ~20s (check power/UART/PWRKEY)\r\n");
    }
    modem_drain_hw_rx();
    s_rx_len = 0U;
    if (modem_probe_online() != 0) {
        modem_set_online_state(1, "offline recovery AT probe", monotonic_ms);
        bsp_debug_log("[4G] modem OK (recovery)\r\n");
        (void)modem_at_simple_ok("ATE0\r\n", 3000U);
        (void)modem_try_refresh_identity();
        (void)modem_try_sync_time();
        return;
    }
    if (s_recovery.cfun_once == 0U) {
        s_recovery.cfun_once = 1U;
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
            modem_set_online_state(1, "offline recovery CFUN", monotonic_ms);
            bsp_debug_log("[4G] modem OK after CFUN (recovery)\r\n");
            (void)modem_at_simple_ok("ATE0\r\n", 3000U);
            (void)modem_try_refresh_identity();
            (void)modem_try_sync_time();
        } else {
            bsp_debug_log("[4G] recovery: AT still dead after CFUN\r\n");
        }
    }
}

bool net_4g_modem_is_online(void)
{
    return s_online != 0;
}

uint32_t net_4g_modem_online_age_ms(uint32_t monotonic_ms)
{
    if (s_online == 0 || monotonic_ms < s_last_online_change_ms) {
        return 0U;
    }
    return monotonic_ms - s_last_online_change_ms;
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

    if (modem_query_cgatt_attached() != 1) {
        bsp_debug_log("[4G] TCP precheck: CGATT not ready, skip QIACT this round\r\n");
        return -1;
    }

    bsp_debug_log("[4G] TCP [2/4] PDP QIACT (attach, may take 45s)\r\n");
    {
        char qiact_last[MODEM_LINE_MAX];
        char qiact_active_line[MODEM_LINE_MAX];
        if (modem_at_simple_ok_capture("AT+QIACT=1\r\n", 45000U, qiact_last, sizeof(qiact_last)) != 0) {
            bsp_debug_log("[4G] TCP [2/4] FAIL: QIACT (SIM/APN/coverage? see diagnostics below)\r\n");
            if (qiact_last[0] != '\0') {
                bsp_debug_log("[4G] TCP [2/4] last modem line before fail:\r\n");
                modem_log_at_rx_line(qiact_last);
            }
            qiact_active_line[0] = '\0';
            if (modem_query_qiact_active(PDP_CONTEXT_ID, qiact_active_line, sizeof(qiact_active_line)) > 0) {
                bsp_debug_log("[4G] TCP [2/4] QIACT? shows PDP already active, continue\r\n");
                if (qiact_active_line[0] != '\0') {
                    bsp_debug_log("[4G] TCP [2/4] active context line:\r\n");
                    modem_log_at_rx_line(qiact_active_line);
                }
            } else {
                modem_dump_qiact_failure_diagnostics();
                return -1;
            }
        }
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

    modem_reset_qird_pending_state();
    s_tcp_connected = 1;
    bsp_debug_log("[4G] TCP [4/4] OK: socket open, data path ready\r\n");
    bsp_debug_log("[4G] TCP mode: AT control + QISEND/QIRD data plane\r\n");
    return 0;
#endif
}

void net_4g_modem_tcp_close(void)
{
#if defined(BOARD_STM32F103)
    modem_reset_qird_pending_state();
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
        {
            char qlog[64];
            (void)snprintf(qlog, sizeof(qlog), "[4G] TCP QISEND payload_len=%u\r\n", (unsigned)len);
            bsp_debug_log(qlog);
        }

        if (modem_wait_substr(">", 5000U) != 0) {
            modem_reset_qird_pending_state();
            s_tcp_connected = 0;
            bsp_debug_log("[4G] TCP: QISEND no prompt\r\n");
            return -1;
        }

        modem_log_tcp_tx(data, len);
        if (bsp_uart_write((int)BOARD_HW_UART_PORT_MODEM_4G, data, len) != (int)len) {
            return -1;
        }

        if (modem_wait_substr("SEND OK", 15000U) != 0) {
            if (modem_wait_line_ok_or_err(3000U) != 0) {
                modem_reset_qird_pending_state();
                s_tcp_connected = 0;
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
    if (s_online != 0 && s_tcp_connected != 0 && s_qird_pending_len > 0U) {
        if (s_qird_pending_retry_after_ms != 0U &&
            (int32_t)(monotonic_ms - s_qird_pending_retry_after_ms) < 0) {
            goto pending_qird_done;
        }
        unsigned request_len = s_qird_pending_len > MODEM_QIRD_FETCH_MAX ? MODEM_QIRD_FETCH_MAX : s_qird_pending_len;
        modem_qird_result_t qird;
        {
            char linebuf[144];
            (void)snprintf(linebuf, sizeof(linebuf),
                           "[4G] QIRD pending fetch start remain=%u request=%u no_data_streak=%u fail_streak=%u\r\n",
                           s_qird_pending_len,
                           request_len,
                           (unsigned)s_qird_pending_no_data_streak,
                           (unsigned)s_qird_fail_streak);
            bsp_debug_log(linebuf);
        }
        int fetch_rc = modem_qird_fetch(request_len, &qird);
        if (fetch_rc < 0) {
            s_qird_fail_streak++;
            s_qird_pending_no_data_streak = 0U;
            s_qird_pending_retry_after_ms = monotonic_ms + MODEM_QIRD_PENDING_RETRY_MS;
            if (s_qird_fail_streak >= 3U) {
                modem_reset_qird_pending_state();
                s_tcp_connected = 0;
                bsp_debug_log("[4G] QIRD pending fetch failed repeatedly, mark TCP disconnected\r\n");
            } else {
                char linebuf[104];
                (void)snprintf(linebuf, sizeof(linebuf),
                               "[4G] QIRD pending fetch failed streak=%u, remain=%u\r\n",
                               (unsigned)s_qird_fail_streak,
                               s_qird_pending_len);
                bsp_debug_log(linebuf);
            }
        } else if (fetch_rc == 0) {
            s_qird_fail_streak = 0U;
            s_qird_pending_no_data_streak++;
            s_qird_pending_retry_after_ms = monotonic_ms + MODEM_QIRD_PENDING_RETRY_MS;
            if (s_qird_pending_no_data_streak >= MODEM_QIRD_PENDING_NO_DATA_LIMIT) {
                char linebuf[160];
                /* A short UART read can leave us with a synthetic "pending" tail even when the
                 * modem socket is still healthy. Drop the stale tail instead of tearing TCP down. */
                (void)snprintf(linebuf, sizeof(linebuf),
                               "[4G] QIRD pending no-data streak=%u remain=%u, drop stale tail and keep TCP alive\r\n",
                               (unsigned)s_qird_pending_no_data_streak,
                               s_qird_pending_len);
                bsp_debug_log(linebuf);
                modem_reset_qird_pending_state();
            } else {
                char linebuf[120];
                (void)snprintf(linebuf, sizeof(linebuf),
                               "[4G] QIRD pending fetch no data streak=%u remain=%u, backoff=%u ms\r\n",
                               (unsigned)s_qird_pending_no_data_streak,
                               s_qird_pending_len,
                               (unsigned)MODEM_QIRD_PENDING_RETRY_MS);
                bsp_debug_log(linebuf);
            }
        } else {
            s_qird_fail_streak = 0U;
            s_qird_pending_no_data_streak = 0U;
            s_qird_pending_retry_after_ms = 0U;
            {
                unsigned next_pending = 0U;
                if (qird.data_len > (unsigned)qird.read_len) {
                    next_pending = (qird.data_len - (unsigned)qird.read_len) + qird.unread_len;
                } else {
                    next_pending = qird.unread_len;
                }
                s_qird_pending_len = next_pending;
            }
            if (s_qird_pending_len == 0U) {
                s_qird_pending_len = 0U;
                bsp_debug_log("[4G] QIRD pending drained fully\r\n");
            } else {
                {
                    char linebuf[104];
                    (void)snprintf(linebuf, sizeof(linebuf),
                                   "[4G] QIRD pending remain=%u after fetch=%d/%u unread=%u\r\n",
                                   s_qird_pending_len,
                                   qird.read_len,
                                   qird.data_len,
                                   qird.unread_len);
                    bsp_debug_log(linebuf);
                }
            }
        }
    }
pending_qird_done:
    modem_offline_recovery(monotonic_ms);
    if (s_online != 0 && modem_identity_is_ready() == 0) {
        if (s_last_identity_attempt_ms == 0U ||
            (uint32_t)(monotonic_ms - s_last_identity_attempt_ms) >= 15000U) {
            s_last_identity_attempt_ms = monotonic_ms;
            bsp_debug_log("[4G] periodic modem identity retry (CGSN/QCCID)\r\n");
            (void)modem_try_refresh_identity();
        }
    }
    if (s_online != 0 && bsp_rtc_is_synced() == 0) {
        if (s_last_time_sync_attempt_ms == 0U ||
            (uint32_t)(monotonic_ms - s_last_time_sync_attempt_ms) >= 15000U) {
            s_last_time_sync_attempt_ms = monotonic_ms;
            bsp_debug_log("[TIME] periodic network time retry (QLTS->CCLK)\r\n");
            (void)modem_try_sync_time();
        }
    }
    if (s_online != 0) {
        if (s_last_signal_attempt_ms == 0U ||
            (uint32_t)(monotonic_ms - s_last_signal_attempt_ms) >= 30000U) {
            s_last_signal_attempt_ms = monotonic_ms;
            (void)modem_try_refresh_signal();
        }
    }
}
