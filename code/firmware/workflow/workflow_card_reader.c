#include "workflow_card_reader.h"
#include "bsp_uart.h"
#include "common_status.h"
#include "net_connectivity.h"
#include "proto_event_report.h"
#include "workflow_engine.h"
#include "workflow_voice.h"

#include <stdio.h>
#include <string.h>

#define WORKFLOW_CARD_READER_UART_PORT ((int)BOARD_HW_UART_PORT_CARD_READER)
#define WORKFLOW_CARD_READER_BUFFER_CAP           128U
#define WORKFLOW_CARD_READER_LINE_CAP             96U
#define WORKFLOW_CARD_READER_GLOBAL_DEBOUNCE_MS   250U
#define WORKFLOW_CARD_READER_SAME_TOKEN_DEBOUNCE_MS 1500U

typedef enum {
    WORKFLOW_CARD_READER_GUARD_ALLOW = 0,
    WORKFLOW_CARD_READER_GUARD_NOT_READY = 1,
    WORKFLOW_CARD_READER_GUARD_BUSY = 2,
    WORKFLOW_CARD_READER_GUARD_STOP_GUARD = 3,
    WORKFLOW_CARD_READER_GUARD_OFFLINE = 4,
    WORKFLOW_CARD_READER_GUARD_FAULT = 5
} workflow_card_reader_guard_t;

static workflow_card_reader_state_t s_state;
static uint8_t                      s_rx_buffer[WORKFLOW_CARD_READER_BUFFER_CAP];
static size_t                       s_rx_len;
static char                         s_last_handled_token[32];
static uint32_t                     s_last_handled_at_ms;

static void workflow_card_reader_copy_symbol(char *dst, size_t cap, const char *src)
{
    size_t idx = 0U;

    if (!dst || cap == 0U) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[idx] != '\0' && idx + 1U < cap) {
        char ch = src[idx];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-') {
            dst[idx] = ch;
        } else {
            dst[idx] = '_';
        }
        idx++;
    }
    dst[idx] = '\0';
}

static const char *workflow_card_reader_state_label(workflow_state_t state)
{
    switch (state) {
    case WF_BOOTING:
        return "booting";
    case WF_ONLINE_NOT_READY:
        return "online_not_ready";
    case WF_READY_IDLE:
        return "ready_idle";
    case WF_STARTING:
        return "starting";
    case WF_RUNNING:
        return "running";
    case WF_PAUSING:
        return "pausing";
    case WF_PAUSED:
        return "paused";
    case WF_RESUMING:
        return "resuming";
    case WF_STOPPING:
        return "stopping";
    case WF_STOPPED:
        return "stopped";
    case WF_ERROR_STOP:
        return "error_stop";
    default:
        return "unknown";
    }
}

static void workflow_card_reader_copy_token_suffix(const char *token)
{
    size_t token_len;
    size_t suffix_len;
    const char *suffix;

    if (!token || token[0] == '\0') {
        s_state.last_token_suffix[0] = '\0';
        return;
    }

    token_len = strlen(token);
    suffix_len = token_len > 6U ? 6U : token_len;
    suffix = token + (token_len - suffix_len);
    workflow_card_reader_copy_symbol(s_state.last_token_suffix, sizeof(s_state.last_token_suffix), suffix);
}

static void workflow_card_reader_set_state(const char *outcome,
                                           const char *reason,
                                           const char *source,
                                           const char *token,
                                           uint32_t    last_swipe_at_ms,
                                           uint32_t    last_reported_at_ms)
{
    workflow_card_reader_copy_symbol(s_state.last_outcome, sizeof(s_state.last_outcome), outcome);
    workflow_card_reader_copy_symbol(s_state.last_reason, sizeof(s_state.last_reason), reason);
    workflow_card_reader_copy_symbol(s_state.last_source, sizeof(s_state.last_source), source);
    workflow_card_reader_copy_token_suffix(token);
    s_state.last_swipe_at_ms = last_swipe_at_ms;
    if (last_reported_at_ms != 0U) {
        s_state.last_reported_at_ms = last_reported_at_ms;
    }
}

static uint8_t workflow_card_reader_checksum_ok(const uint8_t *frame, size_t len)
{
    uint8_t checksum = 0x00U;
    size_t  idx;

    if (!frame || len < 13U) {
        return 0U;
    }

    for (idx = 1U; idx <= 11U; ++idx) {
        checksum ^= frame[idx];
    }
    checksum = (uint8_t)(~checksum);
    return (uint8_t)(checksum == frame[12] ? 1U : 0U);
}

static void workflow_card_reader_token_from_frame(const uint8_t *frame, char *token, size_t token_cap)
{
    uint32_t card_no;

    if (!frame || !token || token_cap == 0U) {
        return;
    }

    card_no = ((uint32_t)frame[8] << 24) | ((uint32_t)frame[9] << 16) | ((uint32_t)frame[10] << 8) | (uint32_t)frame[11];
    (void)snprintf(token, token_cap, "%010lu", (unsigned long)card_no);
}

static int workflow_card_reader_extract_ascii_token(const char *line, char *token, size_t token_cap)
{
    const char *cursor;
    const char *value;
    size_t      length;

    if (!line || !token || token_cap == 0U) {
        return -1;
    }

    cursor = line;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    value = strrchr(cursor, ':');
    if (!value) {
        value = strrchr(cursor, '=');
    }
    if (value) {
        cursor = value + 1;
    }

    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    length = 0U;
    while (cursor[length] != '\0' && cursor[length] != '\r' && cursor[length] != '\n' &&
           cursor[length] != ' ' && cursor[length] != '\t' && length + 1U < token_cap) {
        char ch = cursor[length];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-') {
            token[length] = ch;
        } else {
            return -1;
        }
        length++;
    }

    token[length] = '\0';
    return length >= 4U ? 0 : -1;
}

static void workflow_card_reader_consume(size_t count)
{
    if (count == 0U || s_rx_len == 0U) {
        return;
    }
    if (count >= s_rx_len) {
        s_rx_len = 0U;
        s_state.rx_buffered_bytes = 0U;
        return;
    }

    memmove(s_rx_buffer, s_rx_buffer + count, s_rx_len - count);
    s_rx_len -= count;
    s_state.rx_buffered_bytes = (uint32_t)s_rx_len;
}

static void workflow_card_reader_append_bytes(const uint8_t *data, size_t len)
{
    size_t overflow;

    if (!data || len == 0U) {
        return;
    }

    if (len >= sizeof(s_rx_buffer)) {
        memcpy(s_rx_buffer, data + (len - sizeof(s_rx_buffer)), sizeof(s_rx_buffer));
        s_rx_len = sizeof(s_rx_buffer);
        s_state.frames_invalid += 1U;
        s_state.rx_buffered_bytes = (uint32_t)s_rx_len;
        return;
    }

    if (s_rx_len + len > sizeof(s_rx_buffer)) {
        overflow = (s_rx_len + len) - sizeof(s_rx_buffer);
        memmove(s_rx_buffer, s_rx_buffer + overflow, s_rx_len - overflow);
        s_rx_len -= overflow;
        s_state.frames_invalid += 1U;
    }

    memcpy(s_rx_buffer + s_rx_len, data, len);
    s_rx_len += len;
    s_state.rx_buffered_bytes = (uint32_t)s_rx_len;
}

static void workflow_card_reader_note_handled(const char *token, uint32_t now_ms)
{
    if (!token || token[0] == '\0') {
        s_last_handled_token[0] = '\0';
        s_last_handled_at_ms = now_ms;
        return;
    }

    workflow_card_reader_copy_symbol(s_last_handled_token, sizeof(s_last_handled_token), token);
    s_last_handled_at_ms = now_ms;
}

static workflow_card_reader_guard_t workflow_card_reader_guard(const char *token, uint32_t now_ms, char *reason, size_t reason_cap)
{
    const common_status_t *status = common_status_get();
    device_runtime_t      *runtime = workflow_engine_runtime();
    workflow_state_t       state = workflow_engine_get_state();

    if (s_last_handled_at_ms != 0U) {
        uint32_t delta_ms = now_ms - s_last_handled_at_ms;
        if (delta_ms < WORKFLOW_CARD_READER_GLOBAL_DEBOUNCE_MS) {
            workflow_card_reader_copy_symbol(reason, reason_cap, "global_debounce");
            s_state.debounce_dropped += 1U;
            workflow_card_reader_set_state("debounced", "global_debounce", "uart1_card_reader", token, now_ms, 0U);
            return WORKFLOW_CARD_READER_GUARD_BUSY;
        }
        if (token && token[0] != '\0' && strcmp(token, s_last_handled_token) == 0 &&
            delta_ms < WORKFLOW_CARD_READER_SAME_TOKEN_DEBOUNCE_MS) {
            workflow_card_reader_copy_symbol(reason, reason_cap, "same_token_debounce");
            s_state.debounce_dropped += 1U;
            workflow_card_reader_set_state("debounced", "same_token_debounce", "uart1_card_reader", token, now_ms, 0U);
            return WORKFLOW_CARD_READER_GUARD_BUSY;
        }
    }

    if (!status || !status->online || !status->tcp_connected) {
        workflow_card_reader_copy_symbol(reason, reason_cap, "platform_offline");
        return WORKFLOW_CARD_READER_GUARD_OFFLINE;
    }

    if (status->fault_count > 0U || state == WF_ERROR_STOP) {
        workflow_card_reader_copy_symbol(reason, reason_cap, "controller_fault");
        return WORKFLOW_CARD_READER_GUARD_FAULT;
    }

    if (workflow_engine_stop_guard_remaining_ms() > 0U ||
        (runtime && (runtime->recovery.recovery_pending || runtime->recovery.settlement_pending))) {
        workflow_card_reader_copy_symbol(reason, reason_cap, "stop_guard_active");
        return WORKFLOW_CARD_READER_GUARD_STOP_GUARD;
    }

    if (state == WF_STARTING || state == WF_PAUSING || state == WF_RESUMING || state == WF_STOPPING) {
        workflow_card_reader_copy_symbol(reason, reason_cap, "workflow_transition_busy");
        return WORKFLOW_CARD_READER_GUARD_BUSY;
    }

    if ((state != WF_READY_IDLE && state != WF_RUNNING && state != WF_PAUSED) || (!status->ready && state == WF_READY_IDLE)) {
        workflow_card_reader_copy_symbol(reason, reason_cap, "controller_not_ready");
        return WORKFLOW_CARD_READER_GUARD_NOT_READY;
    }

    workflow_card_reader_copy_symbol(reason, reason_cap, "platform_checkout");
    return WORKFLOW_CARD_READER_GUARD_ALLOW;
}

static void workflow_card_reader_prompt_for_guard(workflow_card_reader_guard_t guard)
{
    static const char *not_ready_prompts[] = {"welcome", "starting_wait"};

    switch (guard) {
    case WORKFLOW_CARD_READER_GUARD_NOT_READY:
        workflow_voice_prompt_sequence("card_reader_guard", not_ready_prompts, 2U, 800U);
        break;
    case WORKFLOW_CARD_READER_GUARD_BUSY:
    case WORKFLOW_CARD_READER_GUARD_STOP_GUARD:
        workflow_voice_prompt_once("port_busy", "card_reader_guard", 1500U);
        break;
    case WORKFLOW_CARD_READER_GUARD_OFFLINE:
        workflow_voice_prompt_once("unavailable", "card_reader_guard", 1500U);
        break;
    case WORKFLOW_CARD_READER_GUARD_FAULT:
        workflow_voice_prompt_once("device_fault", "card_reader_guard", 1500U);
        break;
    default:
        break;
    }
}

static int workflow_card_reader_emit_event(const char *event_code,
                                           const char *token,
                                           const char *reason,
                                           const char *source_code)
{
    char        safe_token[48];
    char        safe_reason[WORKFLOW_CARD_READER_REASON_LEN];
    char        safe_source[WORKFLOW_CARD_READER_SOURCE_LEN];
    char        payload[512];
    char        message[768];
    workflow_state_t state = workflow_engine_get_state();
    const common_status_t *status = common_status_get();
    int         built;

    workflow_card_reader_copy_symbol(safe_token, sizeof(safe_token), token);
    workflow_card_reader_copy_symbol(safe_reason, sizeof(safe_reason), reason);
    workflow_card_reader_copy_symbol(safe_source, sizeof(safe_source), source_code);
    (void)snprintf(payload, sizeof(payload),
                   "\"card_token\":\"%s\","
                   "\"swipe_source\":\"%s\","
                   "\"reader_reason\":\"%s\","
                   "\"workflow_state\":\"%s\","
                   "\"ready\":%s,"
                   "\"tcp_connected\":%s,"
                   "\"stop_guard_remaining_ms\":%lu",
                   safe_token,
                   safe_source,
                   safe_reason,
                   workflow_card_reader_state_label(state),
                   (status && status->ready) ? "true" : "false",
                   (status && status->tcp_connected) ? "true" : "false",
                   (unsigned long)workflow_engine_stop_guard_remaining_ms());

    built = proto_event_report_build(message, sizeof(message), NULL, event_code, payload);
    if (built <= 0) {
        return -1;
    }
    return net_connectivity_send_json(message, (size_t)built);
}

static void workflow_card_reader_emit_rejected(const char *reason, const char *token, uint32_t now_ms)
{
    if (workflow_card_reader_emit_event("card_swipe_rejected", token, reason, "uart1_card_reader") > 0) {
        s_state.reports_sent += 1U;
        s_state.last_reported_at_ms = now_ms;
    } else {
        s_state.reports_failed += 1U;
    }
}

static void workflow_card_reader_handle_token(const char *token, const char *source_code, uint32_t now_ms)
{
    char reason[WORKFLOW_CARD_READER_REASON_LEN];
    workflow_card_reader_guard_t guard;
    int send_result;

    reason[0] = '\0';
    guard = workflow_card_reader_guard(token, now_ms, reason, sizeof(reason));

    if (guard != WORKFLOW_CARD_READER_GUARD_ALLOW) {
        if (strcmp(reason, "global_debounce") != 0 && strcmp(reason, "same_token_debounce") != 0) {
            s_state.rejected_count += 1U;
            workflow_card_reader_set_state("rejected", reason, source_code, token, now_ms, 0U);
            workflow_card_reader_note_handled(token, now_ms);
            workflow_card_reader_emit_rejected(reason, token, now_ms);
            workflow_card_reader_prompt_for_guard(guard);
        }
        return;
    }

    workflow_voice_prompt_once("welcome", "card_reader", 800U);
    send_result = workflow_card_reader_emit_event("card_swipe_requested", token, reason, source_code);
    workflow_card_reader_note_handled(token, now_ms);
    if (send_result > 0) {
        s_state.reports_sent += 1U;
        s_state.last_reported_at_ms = now_ms;
        workflow_card_reader_set_state("reported", reason, source_code, token, now_ms, now_ms);
    } else {
        s_state.reports_failed += 1U;
        workflow_card_reader_set_state("report_failed", "event_report_send_failed", source_code, token, now_ms, 0U);
        workflow_voice_prompt_once("unavailable", "card_reader", 1500U);
    }
}

static size_t workflow_card_reader_try_parse_binary(uint32_t now_ms)
{
    char token[32];

    if (s_rx_len == 0U || s_rx_buffer[0] != 0x20U) {
        return 0U;
    }

    if (s_rx_len < 13U) {
        return 0U;
    }

    if (s_rx_len >= 14U && s_rx_buffer[13] == 0x03U) {
        if (workflow_card_reader_checksum_ok(s_rx_buffer, 13U)) {
            workflow_card_reader_token_from_frame(s_rx_buffer, token, sizeof(token));
            s_state.frames_ok += 1U;
            workflow_card_reader_handle_token(token, "uart1_card_reader", now_ms);
            return 14U;
        }
        s_state.frames_invalid += 1U;
        workflow_card_reader_set_state("parse_invalid", "checksum_failed", "uart1_card_reader", NULL, now_ms, 0U);
        return 1U;
    }

    if (workflow_card_reader_checksum_ok(s_rx_buffer, 13U)) {
        workflow_card_reader_token_from_frame(s_rx_buffer, token, sizeof(token));
        s_state.frames_ok += 1U;
        workflow_card_reader_handle_token(token, "uart1_card_reader", now_ms);
        return 13U;
    }

    s_state.frames_invalid += 1U;
    workflow_card_reader_set_state("parse_invalid", "checksum_failed", "uart1_card_reader", NULL, now_ms, 0U);
    return 1U;
}

static size_t workflow_card_reader_try_parse_ascii(uint32_t now_ms)
{
    size_t idx;
    size_t consume;
    char   line[WORKFLOW_CARD_READER_LINE_CAP];
    char   token[48];

    for (idx = 0U; idx < s_rx_len; ++idx) {
        if (s_rx_buffer[idx] == '\n' || s_rx_buffer[idx] == '\r') {
            break;
        }
    }

    if (idx >= s_rx_len) {
        if (s_rx_len >= sizeof(line) - 1U) {
            s_state.frames_invalid += 1U;
            workflow_card_reader_set_state("parse_invalid", "ascii_line_overflow", "uart_ascii_card_reader", NULL, now_ms, 0U);
            return s_rx_len;
        }
        return 0U;
    }

    if (idx >= sizeof(line)) {
        idx = sizeof(line) - 1U;
    }

    memcpy(line, s_rx_buffer, idx);
    line[idx] = '\0';

    consume = idx + 1U;
    while (consume < s_rx_len && (s_rx_buffer[consume] == '\n' || s_rx_buffer[consume] == '\r')) {
        consume++;
    }

    if (workflow_card_reader_extract_ascii_token(line, token, sizeof(token)) != 0) {
        s_state.frames_invalid += 1U;
        workflow_card_reader_set_state("parse_invalid", "ascii_token_invalid", "uart_ascii_card_reader", NULL, now_ms, 0U);
        return consume;
    }

    s_state.frames_ok += 1U;
    workflow_card_reader_handle_token(token, "uart_ascii_card_reader", now_ms);
    return consume;
}

void workflow_card_reader_init(void)
{
    bsp_uart_card_reader_init();
    memset(&s_state, 0, sizeof(s_state));
    memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    s_rx_len = 0U;
    memset(s_last_handled_token, 0, sizeof(s_last_handled_token));
    s_last_handled_at_ms = 0U;
    s_state.enabled = true;
    s_state.supported = true;
    s_state.uart_port = WORKFLOW_CARD_READER_UART_PORT;
    workflow_card_reader_copy_symbol(s_state.last_outcome, sizeof(s_state.last_outcome), "idle");
}

void workflow_card_reader_tick(uint32_t now_ms)
{
    uint8_t chunk[32];
    int     read_bytes;
    size_t  consumed;

    do {
        read_bytes = bsp_uart_read((int)s_state.uart_port, chunk, sizeof(chunk));
        if (read_bytes > 0) {
            workflow_card_reader_append_bytes(chunk, (size_t)read_bytes);
        }
    } while (read_bytes > 0);

    while (s_rx_len > 0U) {
        consumed = 0U;

        if (s_rx_buffer[0] == 0x20U) {
            consumed = workflow_card_reader_try_parse_binary(now_ms);
            if (consumed == 0U && s_rx_len < 13U) {
                break;
            }
        } else {
            consumed = workflow_card_reader_try_parse_ascii(now_ms);
        }

        if (consumed == 0U) {
            if (s_rx_buffer[0] >= 0x20U && s_rx_buffer[0] <= 0x7EU) {
                break;
            }
            workflow_card_reader_consume(1U);
            continue;
        }

        workflow_card_reader_consume(consumed);
    }
}

void workflow_card_reader_get_state(workflow_card_reader_state_t *out)
{
    if (!out) {
        return;
    }
    s_state.rx_buffered_bytes = (uint32_t)s_rx_len;
    *out = s_state;
}
