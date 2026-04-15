#include "module_meter.h"

#include "app_context.h"
#include "bsp_system.h"
#include "bsp_uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MODULE_METER_RS485_BAUD_DEFAULT    2400U
#define MODULE_METER_RX_BUF_CAP             128U
#define MODULE_METER_FRAME_CAP               32U
#define MODULE_METER_RESPONSE_TIMEOUT_MS    800U
#define MODULE_METER_NEXT_REQ_DELAY_MS      150U
#define MODULE_METER_DISCOVER_RETRY_MS     3000U
#define MODULE_METER_STALE_MS             15000U
#define MODULE_METER_MAX_TIMEOUT_STREAK      4U
#define MODULE_METER_ACTION_WAIT_MS       1500U
#define MODULE_METER_ACTION_POLL_MS         50U
#define MODULE_METER_ACTION_MAX_RETRY        2U

typedef enum {
    MODULE_METER_REQ_NONE = 0,
    MODULE_METER_REQ_DISCOVER_ADDR,
    MODULE_METER_REQ_ENERGY_TOTAL,
    MODULE_METER_REQ_POWER_TOTAL,
    MODULE_METER_REQ_VOLTAGE_A,
    MODULE_METER_REQ_CURRENT_A
} module_meter_request_t;

typedef struct {
    module_meter_request_t kind;
    uint8_t                di[4];
    const char            *name;
} module_meter_query_t;

typedef enum {
    MODULE_METER_ACT_NONE = 0,
    MODULE_METER_ACT_CLOSE_BREAKER,
    MODULE_METER_ACT_OPEN_BREAKER
} module_meter_action_t;

typedef enum {
    MODULE_METER_ACTION_IDLE = 0,
    MODULE_METER_ACTION_PENDING,
    MODULE_METER_ACTION_SUCCESS,
    MODULE_METER_ACTION_FAILED
} module_meter_action_status_t;

typedef enum {
    MODULE_METER_ACTION_FAIL_NONE = 0,
    MODULE_METER_ACTION_FAIL_TIMEOUT,
    MODULE_METER_ACTION_FAIL_WRITE,
    MODULE_METER_ACTION_FAIL_UNSUPPORTED
} module_meter_action_fail_reason_t;

typedef struct {
    uint8_t                       addr_bcd[6];
    uint8_t                       addr_valid;
    uint8_t                       waiting;
    uint8_t                       timeout_streak;
    uint8_t                       query_index;
    uint32_t                      next_action_ms;
    uint32_t                      response_deadline_ms;
    uint32_t                      last_good_ms;
    module_meter_request_t        pending;
    module_meter_action_t         action_pending;
    module_meter_action_status_t  action_status;
    module_meter_action_fail_reason_t action_fail_reason;
    uint8_t                       rx_buf[MODULE_METER_RX_BUF_CAP];
    size_t                        rx_len;
} module_meter_link_t;

static module_meter_config_t s_cfg;
static module_meter_values_t s_val;
static module_meter_link_t   s_link;

static const module_meter_query_t s_queries[] = {
    { MODULE_METER_REQ_ENERGY_TOTAL, {0x00U, 0x00U, 0x00U, 0x00U}, "energy_total" },
    { MODULE_METER_REQ_POWER_TOTAL,  {0x00U, 0x00U, 0x03U, 0x02U}, "power_total"  },
    { MODULE_METER_REQ_VOLTAGE_A,    {0x00U, 0xFFU, 0x01U, 0x02U}, "voltage_abc"  },
    { MODULE_METER_REQ_CURRENT_A,    {0x00U, 0xFFU, 0x02U, 0x02U}, "current_abc"  },
};

static void meter_consume_uart(void);
static int meter_extract_frame(uint8_t *frame, size_t *frame_len);
static void meter_handle_response(const uint8_t *frame, size_t frame_len);
static const module_meter_query_t *meter_query_by_kind(module_meter_request_t kind);

static const uint8_t s_meter_close_breaker_tpl[32] = {
    0xFEU, 0xFEU, 0xFEU, 0xFEU,
    0x68U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x68U,
    0x1CU, 0x10U, 0x35U, 0x7AU, 0xB4U, 0x8BU,
    0x33U, 0x33U, 0x33U, 0x33U, 0x4FU, 0x33U, 0x8CU, 0x8CU,
    0x56U, 0x54U, 0x45U, 0x53U, 0x30U, 0x16U
};

static const uint8_t s_meter_open_breaker_tpl[32] = {
    0xFEU, 0xFEU, 0xFEU, 0xFEU,
    0x68U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x68U,
    0x1CU, 0x10U, 0x35U, 0x7AU, 0xB4U, 0x8BU,
    0x33U, 0x33U, 0x33U, 0x33U, 0x4DU, 0x33U, 0x8CU, 0x8CU,
    0x56U, 0x54U, 0x45U, 0x53U, 0x30U, 0x16U
};

static uint8_t meter_protocol_active(void)
{
    return (s_cfg.protocol_variant == MODULE_METER_PROTOCOL_UNKNOWN)
        ? MODULE_METER_PROTOCOL_DLT645_2007
        : s_cfg.protocol_variant;
}

static uint32_t meter_now_ms(void)
{
    return app_context()->monotonic_ms;
}

static void meter_log_line(const char *fmt, ...)
{
    char line[160];
    va_list ap;

    va_start(ap, fmt);
    (void)vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    bsp_debug_log(line);
}

static void meter_log_addr(const char *prefix, const uint8_t addr[6])
{
    char line[128];

    (void)snprintf(line, sizeof(line),
                   "[METER] %s addr=%02X%02X%02X%02X%02X%02X\r\n",
                   prefix,
                   addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    bsp_debug_log(line);
}

static uint8_t meter_addr_is_nonzero(const uint8_t addr[6])
{
    size_t i;

    for (i = 0U; i < 6U; i++) {
        if (addr[i] != 0U) {
            return 1U;
        }
    }
    return 0U;
}

static uint8_t meter_checksum(const uint8_t *buf, size_t len)
{
    size_t i;
    uint8_t sum = 0U;

    for (i = 0U; i < len; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }
    return sum;
}

static int meter_send_frame(const uint8_t addr[6], uint8_t ctrl, const uint8_t *data, uint8_t data_len)
{
    uint8_t frame[MODULE_METER_FRAME_CAP];
    size_t i;
    size_t frame_len;

    if (addr == NULL) {
        return -1;
    }
    frame[0] = 0x68U;
    for (i = 0U; i < 6U; i++) {
        frame[1U + i] = addr[i];
    }
    frame[7] = 0x68U;
    frame[8] = ctrl;
    frame[9] = data_len;
    for (i = 0U; i < data_len; i++) {
        frame[10U + i] = (uint8_t)(data[i] + 0x33U);
    }
    frame[10U + data_len] = meter_checksum(frame, (size_t)(10U + data_len));
    frame[11U + data_len] = 0x16U;
    frame_len = (size_t)(12U + data_len);

    if (bsp_uart_write((int)BOARD_HW_UART_PORT_RS485, frame, frame_len) != (int)frame_len) {
        return -2;
    }
    return 0;
}

static int meter_send_read_addr(void)
{
    static const uint8_t broadcast_addr[6] = {0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU};
    return meter_send_frame(broadcast_addr, 0x13U, NULL, 0U);
}

static int meter_send_query(const module_meter_query_t *query)
{
    if (query == NULL || s_link.addr_valid == 0U) {
        return -1;
    }
    return meter_send_frame(s_link.addr_bcd, 0x11U, query->di, 4U);
}

static int meter_send_breaker_action(module_meter_action_t action)
{
    uint8_t frame[32];
    uint8_t sum = 0U;
    size_t i;

    if (s_link.addr_valid == 0U || meter_protocol_active() != MODULE_METER_PROTOCOL_DLT645_2007) {
        s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_UNSUPPORTED;
        return -1;
    }

    if (action == MODULE_METER_ACT_CLOSE_BREAKER) {
        memcpy(frame, s_meter_close_breaker_tpl, sizeof(frame));
    } else if (action == MODULE_METER_ACT_OPEN_BREAKER) {
        memcpy(frame, s_meter_open_breaker_tpl, sizeof(frame));
    } else {
        s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_UNSUPPORTED;
        return -1;
    }

    for (i = 0U; i < 6U; i++) {
        frame[5U + i] = s_link.addr_bcd[i];
    }
    for (i = 0U; i < 26U; i++) {
        sum = (uint8_t)(sum + frame[4U + i]);
    }
    frame[30] = sum;

    if (bsp_uart_write((int)BOARD_HW_UART_PORT_RS485, frame, sizeof(frame)) != (int)sizeof(frame)) {
        s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_WRITE;
        return -2;
    }
    s_link.action_pending = action;
    s_link.action_status = MODULE_METER_ACTION_PENDING;
    s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_NONE;
    return 0;
}

static uint8_t meter_wait_query_result(module_meter_request_t expected_kind, uint32_t wait_ms)
{
    uint32_t waited_ms = 0U;

    while (waited_ms < wait_ms) {
        meter_consume_uart();
        {
            uint8_t frame[MODULE_METER_FRAME_CAP];
            size_t frame_len = 0U;

            while (meter_extract_frame(frame, &frame_len) != 0) {
                meter_handle_response(frame, frame_len);
                if (expected_kind == MODULE_METER_REQ_DISCOVER_ADDR) {
                    if (s_link.addr_valid != 0U && s_link.pending == MODULE_METER_REQ_NONE) {
                        return 0U;
                    }
                } else if (s_link.pending == MODULE_METER_REQ_NONE) {
                    return 0U;
                }
            }
        }
        bsp_system_delay_ms(MODULE_METER_ACTION_POLL_MS);
        waited_ms += MODULE_METER_ACTION_POLL_MS;
    }

    s_link.waiting = 0U;
    s_link.pending = MODULE_METER_REQ_NONE;
    return 1U;
}

static uint8_t meter_run_query_sync(module_meter_request_t kind)
{
    const module_meter_query_t *query = NULL;
    int rc;

    meter_consume_uart();
    s_link.rx_len = 0U;
    s_link.waiting = 0U;
    s_link.pending = MODULE_METER_REQ_NONE;

    if (kind == MODULE_METER_REQ_DISCOVER_ADDR) {
        rc = meter_send_read_addr();
    } else {
        query = meter_query_by_kind(kind);
        if (query == NULL || s_link.addr_valid == 0U) {
            return 1U;
        }
        rc = meter_send_query(query);
    }
    if (rc != 0) {
        return 1U;
    }

    s_link.pending = kind;
    s_link.waiting = 1U;
    return meter_wait_query_result(kind, MODULE_METER_ACTION_WAIT_MS);
}

static void meter_begin_action(module_meter_action_t action)
{
    s_link.waiting = 0U;
    s_link.pending = MODULE_METER_REQ_NONE;
    s_link.rx_len = 0U;
    s_link.action_pending = action;
    s_link.action_status = MODULE_METER_ACTION_PENDING;
    s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_NONE;
    meter_consume_uart();
    s_link.rx_len = 0U;
}

static uint8_t meter_wait_action_result(module_meter_action_t action)
{
    uint32_t waited_ms = 0U;

    s_link.action_pending = action;
    s_link.action_status = MODULE_METER_ACTION_PENDING;
    s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_NONE;

    while (waited_ms < MODULE_METER_ACTION_WAIT_MS) {
        meter_consume_uart();
        {
            uint8_t frame[MODULE_METER_FRAME_CAP];
            size_t frame_len = 0U;

            while (meter_extract_frame(frame, &frame_len) != 0) {
                meter_handle_response(frame, frame_len);
                if (s_link.action_status == MODULE_METER_ACTION_SUCCESS) {
                    s_link.action_pending = MODULE_METER_ACT_NONE;
                    return 0U;
                }
            }
        }
        bsp_system_delay_ms(MODULE_METER_ACTION_POLL_MS);
        waited_ms += MODULE_METER_ACTION_POLL_MS;
    }

    s_link.action_status = MODULE_METER_ACTION_FAILED;
    s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_TIMEOUT;
    s_link.action_pending = MODULE_METER_ACT_NONE;
    return 1U;
}

static void meter_consume_uart(void)
{
    uint8_t tmp[32];
    int n;

    do {
        n = bsp_uart_read((int)BOARD_HW_UART_PORT_RS485, tmp, sizeof(tmp));
        if (n > 0) {
            size_t copy = (size_t)n;

            if (copy > (sizeof(s_link.rx_buf) - s_link.rx_len)) {
                if (copy > sizeof(s_link.rx_buf)) {
                    copy = sizeof(s_link.rx_buf);
                }
                memmove(s_link.rx_buf,
                        s_link.rx_buf + (s_link.rx_len + copy - sizeof(s_link.rx_buf)),
                        sizeof(s_link.rx_buf) - copy);
                s_link.rx_len = sizeof(s_link.rx_buf) - copy;
            }
            memcpy(s_link.rx_buf + s_link.rx_len, tmp, copy);
            s_link.rx_len += copy;
        }
    } while (n > 0);
}

static int meter_extract_frame(uint8_t *frame, size_t *frame_len)
{
    size_t i;

    if (frame == NULL || frame_len == NULL) {
        return 0;
    }

    for (i = 0U; i + 12U <= s_link.rx_len; i++) {
        size_t candidate_len;
        uint8_t data_len;

        if (s_link.rx_buf[i] != 0x68U) {
            continue;
        }
        if (s_link.rx_buf[i + 7U] != 0x68U) {
            continue;
        }
        data_len = s_link.rx_buf[i + 9U];
        candidate_len = (size_t)(12U + data_len);
        if (i + candidate_len > s_link.rx_len) {
            break;
        }
        if (s_link.rx_buf[i + candidate_len - 1U] != 0x16U) {
            continue;
        }
        if (meter_checksum(&s_link.rx_buf[i], candidate_len - 2U) != s_link.rx_buf[i + candidate_len - 2U]) {
            continue;
        }
        if (candidate_len > MODULE_METER_FRAME_CAP) {
            memmove(s_link.rx_buf, s_link.rx_buf + i + candidate_len, s_link.rx_len - i - candidate_len);
            s_link.rx_len -= (i + candidate_len);
            return 0;
        }

        memcpy(frame, &s_link.rx_buf[i], candidate_len);
        *frame_len = candidate_len;
        memmove(s_link.rx_buf, s_link.rx_buf + i + candidate_len, s_link.rx_len - i - candidate_len);
        s_link.rx_len -= (i + candidate_len);
        return 1;
    }

    if (s_link.rx_len > 10U) {
        memmove(s_link.rx_buf, s_link.rx_buf + (s_link.rx_len - 10U), 10U);
        s_link.rx_len = 10U;
    }
    return 0;
}

static void meter_decode_data_field(uint8_t *dst, const uint8_t *src, size_t len)
{
    size_t i;

    for (i = 0U; i < len; i++) {
        dst[i] = (uint8_t)(src[i] - 0x33U);
    }
}

static uint8_t meter_decode_bcd_le(const uint8_t *src, size_t len, uint32_t *out)
{
    size_t i;
    uint32_t scale = 1U;
    uint32_t value = 0U;

    if (src == NULL || out == NULL) {
        return 0U;
    }
    for (i = 0U; i < len; i++) {
        uint8_t low = (uint8_t)(src[i] & 0x0FU);
        uint8_t high = (uint8_t)((src[i] >> 4U) & 0x0FU);

        if (low > 9U || high > 9U) {
            return 0U;
        }
        value += (uint32_t)low * scale;
        scale *= 10U;
        value += (uint32_t)high * scale;
        scale *= 10U;
    }
    *out = value;
    return 1U;
}

static uint8_t meter_decode_phase_values(const uint8_t *payload, size_t groups, uint8_t bytes_per_group, double scale, double *out_avg)
{
    size_t i;
    uint32_t raw = 0U;
    double sum = 0.0;
    uint32_t count = 0U;

    if (payload == NULL || out_avg == NULL) {
        return 0U;
    }
    for (i = 0U; i < groups; i++) {
        if (meter_decode_bcd_le(payload + (i * bytes_per_group), bytes_per_group, &raw) != 0U) {
            if (raw > 0U) {
                sum += (double)raw * scale;
                count++;
            }
        }
    }
    if (count == 0U) {
        return 0U;
    }
    *out_avg = sum / (double)count;
    return 1U;
}

static const module_meter_query_t *meter_query_by_kind(module_meter_request_t kind)
{
    size_t i;

    for (i = 0U; i < (sizeof(s_queries) / sizeof(s_queries[0])); i++) {
        if (s_queries[i].kind == kind) {
            return &s_queries[i];
        }
    }
    return NULL;
}

static void meter_apply_query_value(module_meter_request_t kind, const uint8_t *payload, size_t payload_len)
{
    uint32_t raw = 0U;
    uint8_t tmp[4];
    double value;

    if (payload == NULL) {
        return;
    }

    switch (kind) {
    case MODULE_METER_REQ_ENERGY_TOTAL:
        if (payload_len < 4U || meter_decode_bcd_le(payload, 4U, &raw) == 0U) {
            return;
        }
        s_val.energy_kwh = (double)raw / 100.0;
        break;
    case MODULE_METER_REQ_POWER_TOTAL:
        if (payload_len < 3U) {
            return;
        }
        memcpy(tmp, payload, 3U);
        if ((tmp[2] & 0x80U) != 0U) {
            tmp[2] &= 0x7FU;
        }
        if (meter_decode_bcd_le(tmp, 3U, &raw) == 0U) {
            return;
        }
        value = (double)raw / 10.0;
        s_val.power_kw = value >= 0.0 ? value : 0.0;
        break;
    case MODULE_METER_REQ_VOLTAGE_A:
        if (payload_len < 6U || meter_decode_phase_values(payload, 3U, 2U, 0.1, &value) == 0U) {
            return;
        }
        s_val.voltage_v = value;
        break;
    case MODULE_METER_REQ_CURRENT_A:
        if (payload_len < 9U || meter_decode_phase_values(payload, 3U, 3U, 0.001, &value) == 0U) {
            return;
        }
        s_val.current_a = value;
        break;
    default:
        return;
    }

    s_val.quality = 1U;
    s_link.last_good_ms = meter_now_ms();
    s_link.timeout_streak = 0U;
}

static void meter_handle_response(const uint8_t *frame, size_t frame_len)
{
    uint8_t ctrl;
    uint8_t data_len;
    uint8_t decoded[16];

    if (frame == NULL || frame_len < 12U) {
        return;
    }

    ctrl = frame[8];
    data_len = frame[9];
    if ((size_t)(12U + data_len) != frame_len) {
        return;
    }

    if (ctrl == 0x9CU) {
        if (s_link.action_pending == MODULE_METER_ACT_CLOSE_BREAKER ||
            s_link.action_pending == MODULE_METER_ACT_OPEN_BREAKER) {
            s_link.action_status = MODULE_METER_ACTION_SUCCESS;
        }
        return;
    }

    if (ctrl == 0x93U && s_link.pending == MODULE_METER_REQ_DISCOVER_ADDR && data_len == 6U) {
        meter_decode_data_field(s_link.addr_bcd, &frame[10], 6U);
        if (meter_addr_is_nonzero(s_link.addr_bcd) != 0U) {
            s_link.addr_valid = 1U;
            s_link.waiting = 0U;
            s_link.pending = MODULE_METER_REQ_NONE;
            s_link.next_action_ms = meter_now_ms() + MODULE_METER_NEXT_REQ_DELAY_MS;
            s_link.timeout_streak = 0U;
            meter_log_addr("DLT645 address discovered", s_link.addr_bcd);
        }
        return;
    }

    if (ctrl != 0x91U || data_len < 4U) {
        return;
    }

    meter_decode_data_field(decoded, &frame[10], data_len);
    if (s_link.pending != MODULE_METER_REQ_NONE) {
        const module_meter_query_t *query = meter_query_by_kind(s_link.pending);

        if (query != NULL && memcmp(decoded, query->di, 4U) == 0) {
            meter_apply_query_value(s_link.pending, decoded + 4U, (size_t)(data_len - 4U));
            s_link.waiting = 0U;
            s_link.pending = MODULE_METER_REQ_NONE;
            s_link.query_index = (uint8_t)((s_link.query_index + 1U) % (sizeof(s_queries) / sizeof(s_queries[0])));
            s_link.next_action_ms = meter_now_ms() + MODULE_METER_NEXT_REQ_DELAY_MS;
        }
    }
}

static void meter_pump_frames(void)
{
    uint8_t frame[MODULE_METER_FRAME_CAP];
    size_t frame_len = 0U;

    meter_consume_uart();
    while (meter_extract_frame(frame, &frame_len) != 0) {
        meter_handle_response(frame, frame_len);
    }
}

static void meter_send_next_query(void)
{
    int rc;

    if (meter_protocol_active() != MODULE_METER_PROTOCOL_DLT645_2007) {
        return;
    }

    if (s_link.addr_valid == 0U) {
        rc = meter_send_read_addr();
        if (rc == 0) {
            s_link.pending = MODULE_METER_REQ_DISCOVER_ADDR;
            s_link.waiting = 1U;
            s_link.response_deadline_ms = meter_now_ms() + MODULE_METER_RESPONSE_TIMEOUT_MS;
        } else {
            s_link.next_action_ms = meter_now_ms() + MODULE_METER_DISCOVER_RETRY_MS;
        }
        return;
    }

    {
        const module_meter_query_t *query = &s_queries[s_link.query_index % (sizeof(s_queries) / sizeof(s_queries[0]))];

        rc = meter_send_query(query);
        if (rc == 0) {
            s_link.pending = query->kind;
            s_link.waiting = 1U;
            s_link.response_deadline_ms = meter_now_ms() + MODULE_METER_RESPONSE_TIMEOUT_MS;
        } else {
            s_link.next_action_ms = meter_now_ms() + MODULE_METER_DISCOVER_RETRY_MS;
        }
    }
}

static void meter_handle_timeout(void)
{
    uint32_t now = meter_now_ms();

    if (s_link.waiting != 0U && (int32_t)(now - s_link.response_deadline_ms) >= 0) {
        s_link.waiting = 0U;
        s_link.pending = MODULE_METER_REQ_NONE;
        s_link.timeout_streak++;
        if (s_link.addr_valid == 0U) {
            s_link.next_action_ms = now + MODULE_METER_DISCOVER_RETRY_MS;
        } else {
            s_link.query_index = (uint8_t)((s_link.query_index + 1U) % (sizeof(s_queries) / sizeof(s_queries[0])));
            s_link.next_action_ms = now + MODULE_METER_NEXT_REQ_DELAY_MS;
        }
        if (s_link.timeout_streak >= MODULE_METER_MAX_TIMEOUT_STREAK) {
            s_link.addr_valid = 0U;
            s_val.quality = 0U;
            s_link.next_action_ms = now + MODULE_METER_DISCOVER_RETRY_MS;
            meter_log_line("[METER] DLT645 timeout streak=%u, restart address discovery\r\n",
                           (unsigned)s_link.timeout_streak);
            s_link.timeout_streak = 0U;
        }
    }

    if (s_val.quality != 0U && (int32_t)(now - s_link.last_good_ms) >= (int32_t)MODULE_METER_STALE_MS) {
        s_val.quality = 0U;
    }
}

void module_meter_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_val, 0, sizeof(s_val));
    memset(&s_link, 0, sizeof(s_link));
    s_cfg.baudrate = MODULE_METER_RS485_BAUD_DEFAULT;
    s_cfg.protocol_variant = MODULE_METER_PROTOCOL_DLT645_2007;
    bsp_uart_rs485_init(s_cfg.baudrate);
}

void module_meter_tick_100ms(void)
{
    if (meter_protocol_active() != MODULE_METER_PROTOCOL_DLT645_2007) {
        return;
    }

    meter_pump_frames();
    meter_handle_timeout();

    if (s_link.waiting == 0U && (int32_t)(meter_now_ms() - s_link.next_action_ms) >= 0) {
        meter_send_next_query();
    }
}

void module_meter_tick_1s(void)
{
    meter_handle_timeout();
}

uint8_t module_meter_apply_config(const module_meter_config_t *cfg)
{
    if (cfg == NULL) {
        return 1U;
    }

    s_cfg = *cfg;
    if (s_cfg.baudrate == 0U) {
        s_cfg.baudrate = MODULE_METER_RS485_BAUD_DEFAULT;
    }
    if (s_cfg.protocol_variant == MODULE_METER_PROTOCOL_UNKNOWN) {
        s_cfg.protocol_variant = MODULE_METER_PROTOCOL_DLT645_2007;
    }

    memset(&s_link, 0, sizeof(s_link));
    if (meter_addr_is_nonzero(s_cfg.addr_bcd) != 0U) {
        memcpy(s_link.addr_bcd, s_cfg.addr_bcd, sizeof(s_link.addr_bcd));
        s_link.addr_valid = 1U;
        meter_log_addr("use configured DLT645 address", s_link.addr_bcd);
    }
    bsp_uart_rs485_init(s_cfg.baudrate);
    return 0U;
}

uint8_t module_meter_query_state(void *out)
{
    (void)out;
    return 0U;
}

uint8_t module_meter_query_values(void *out)
{
    if (out == NULL) {
        return 1U;
    }
    *(module_meter_values_t *)out = s_val;
    return 0U;
}

uint8_t module_meter_force_refresh(void)
{
    uint8_t refreshed = 0U;
    size_t i;

    if (meter_protocol_active() != MODULE_METER_PROTOCOL_DLT645_2007) {
        return 1U;
    }

    if (s_link.addr_valid == 0U) {
        if (meter_run_query_sync(MODULE_METER_REQ_DISCOVER_ADDR) != 0U) {
            meter_log_line("[METER] force refresh failed: address unknown\r\n");
            return 1U;
        }
        refreshed = 1U;
    }

    for (i = 0U; i < (sizeof(s_queries) / sizeof(s_queries[0])); i++) {
        if (meter_run_query_sync(s_queries[i].kind) == 0U) {
            refreshed = 1U;
        }
    }

    meter_log_line("[METER] force refresh %s proto=%s quality=%u\r\n",
                   refreshed != 0U ? "done" : "failed",
                   module_meter_source_name(),
                   (unsigned)s_val.quality);
    return refreshed != 0U ? 0U : 1U;
}

uint8_t module_meter_execute_action(const char *action_code, const char *target_ref, const void *payload)
{
    char line[160];
    int rc;
    module_meter_action_t action = MODULE_METER_ACT_NONE;
    uint8_t attempt;

    (void)payload;
    if (action_code == NULL) {
        return 1U;
    }

    if (strcmp(action_code, "close_breaker") == 0) {
        action = MODULE_METER_ACT_CLOSE_BREAKER;
    } else if (strcmp(action_code, "open_breaker") == 0) {
        action = MODULE_METER_ACT_OPEN_BREAKER;
    } else {
        s_link.action_fail_reason = MODULE_METER_ACTION_FAIL_UNSUPPORTED;
    }

    if (action != MODULE_METER_ACT_NONE) {
        for (attempt = 0U; attempt < MODULE_METER_ACTION_MAX_RETRY; attempt++) {
            meter_begin_action(action);
            rc = meter_send_breaker_action(action);
            if (rc == 0 && meter_wait_action_result(action) == 0U) {
                if (action == MODULE_METER_ACT_CLOSE_BREAKER) {
                    (void)module_meter_force_refresh();
                }
                meter_log_line("[METER] breaker action success action=%s target=%s proto=%s\r\n",
                               action_code,
                               target_ref != NULL ? target_ref : "",
                               module_meter_source_name());
                return 0U;
            }
            meter_log_line("[METER] breaker action retry action=%s target=%s proto=%s attempt=%u reason=%s\r\n",
                           action_code,
                           target_ref != NULL ? target_ref : "",
                           module_meter_source_name(),
                           (unsigned)(attempt + 1U),
                           s_link.action_fail_reason == MODULE_METER_ACTION_FAIL_TIMEOUT ? "timeout"
                               : s_link.action_fail_reason == MODULE_METER_ACTION_FAIL_WRITE ? "write"
                               : s_link.action_fail_reason == MODULE_METER_ACTION_FAIL_UNSUPPORTED ? "unsupported"
                               : "unknown");
        }
    }

    (void)snprintf(line, sizeof(line),
                   "[METER] breaker action failed action=%s target=%s proto=%s reason=%s\r\n",
                   action_code,
                   target_ref != NULL ? target_ref : "",
                   module_meter_source_name(),
                   s_link.action_fail_reason == MODULE_METER_ACTION_FAIL_TIMEOUT ? "timeout"
                       : s_link.action_fail_reason == MODULE_METER_ACTION_FAIL_WRITE ? "write"
                       : s_link.action_fail_reason == MODULE_METER_ACTION_FAIL_UNSUPPORTED ? "unsupported"
                       : "unknown");
    bsp_debug_log(line);
    return 1U;
}

const char *module_meter_source_name(void)
{
    return meter_protocol_active() == MODULE_METER_PROTOCOL_DLT645_2007
        ? "dlt645_2007"
        : "unknown";
}

uint8_t module_meter_get_identity(uint8_t *protocol_variant, uint8_t addr_bcd[6], uint8_t *addr_valid)
{
    if (protocol_variant != NULL) {
        *protocol_variant = meter_protocol_active();
    }
    if (addr_valid != NULL) {
        *addr_valid = s_link.addr_valid;
    }
    if (addr_bcd != NULL) {
        if (s_link.addr_valid != 0U) {
            memcpy(addr_bcd, s_link.addr_bcd, sizeof(s_link.addr_bcd));
        } else {
            memset(addr_bcd, 0, sizeof(s_link.addr_bcd));
        }
    }
    return 0U;
}

static const module_ops_t s_ops = {
    .module_code    = "electric_meter_modbus",
    .init           = module_meter_init,
    .tick_100ms     = module_meter_tick_100ms,
    .tick_1s        = module_meter_tick_1s,
    .apply_config   = (uint8_t (*)(const void *))module_meter_apply_config,
    .query_state    = module_meter_query_state,
    .query_values   = module_meter_query_values,
    .execute_action = module_meter_execute_action,
};

const module_ops_t *module_meter_ops(void)
{
    return &s_ops;
}
