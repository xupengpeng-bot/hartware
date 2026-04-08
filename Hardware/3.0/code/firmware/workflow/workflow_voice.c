#include "workflow_voice.h"
#include "bsp_voice.h"

#include <string.h>

#define WORKFLOW_VOICE_QUEUE_CAP 8U
#define WORKFLOW_VOICE_BUSY_HOLD_MS 1200U

typedef struct {
    char prompt[WORKFLOW_VOICE_PROMPT_LEN];
    char source[WORKFLOW_VOICE_SOURCE_LEN];
} workflow_voice_queue_item_t;

static workflow_voice_queue_item_t s_queue[WORKFLOW_VOICE_QUEUE_CAP];
static uint8_t                     s_queue_head;
static uint8_t                     s_queue_len;
static uint32_t                    s_now_ms;
static uint32_t                    s_busy_until_ms;
static workflow_voice_state_t      s_state;

static void workflow_voice_copy_symbol(char *dst, size_t cap, const char *src)
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

static void workflow_voice_queue_push(const char *prompt_code, const char *source_code)
{
    workflow_voice_queue_item_t *item;
    uint8_t                      slot;

    if (!prompt_code || prompt_code[0] == '\0') {
        return;
    }

    if (s_queue_len >= WORKFLOW_VOICE_QUEUE_CAP) {
        s_queue_head = (uint8_t)((s_queue_head + 1U) % WORKFLOW_VOICE_QUEUE_CAP);
        s_queue_len--;
    }

    slot = (uint8_t)((s_queue_head + s_queue_len) % WORKFLOW_VOICE_QUEUE_CAP);
    item = &s_queue[slot];
    workflow_voice_copy_symbol(item->prompt, sizeof(item->prompt), prompt_code);
    workflow_voice_copy_symbol(item->source, sizeof(item->source), source_code);
    s_queue_len++;
    s_state.queue_depth = s_queue_len;
}

static bool workflow_voice_queue_pop(workflow_voice_queue_item_t *out)
{
    if (!out || s_queue_len == 0U) {
        return false;
    }

    *out = s_queue[s_queue_head];
    s_queue_head = (uint8_t)((s_queue_head + 1U) % WORKFLOW_VOICE_QUEUE_CAP);
    s_queue_len--;
    s_state.queue_depth = s_queue_len;
    return true;
}

void workflow_voice_init(void)
{
    memset(s_queue, 0, sizeof(s_queue));
    s_queue_head = 0U;
    s_queue_len = 0U;
    s_now_ms = 0U;
    s_busy_until_ms = 0U;
    memset(&s_state, 0, sizeof(s_state));
    s_state.enabled = true;
    s_state.supported = bsp_voice_supported();
    bsp_voice_init();
}

void workflow_voice_tick(uint32_t now_ms)
{
    workflow_voice_queue_item_t item;

    s_now_ms = now_ms;
    s_state.supported = bsp_voice_supported();
    s_state.busy = workflow_voice_is_busy();
    s_state.queue_depth = s_queue_len;

    if (workflow_voice_is_busy()) {
        return;
    }

    if (!workflow_voice_queue_pop(&item)) {
        return;
    }

    (void)bsp_voice_play_prompt(item.prompt);
    workflow_voice_copy_symbol(s_state.last_prompt, sizeof(s_state.last_prompt), item.prompt);
    workflow_voice_copy_symbol(s_state.last_source, sizeof(s_state.last_source), item.source);
    s_state.last_prompt_at_ms = now_ms;
    s_busy_until_ms = now_ms + WORKFLOW_VOICE_BUSY_HOLD_MS;
    s_state.busy = true;
}

void workflow_voice_clear(void)
{
    memset(s_queue, 0, sizeof(s_queue));
    s_queue_head = 0U;
    s_queue_len = 0U;
    s_busy_until_ms = 0U;
    s_state.queue_depth = 0U;
    s_state.busy = false;
}

bool workflow_voice_is_busy(void)
{
    return bsp_voice_is_busy() || (s_busy_until_ms != 0U && (int32_t)(s_busy_until_ms - s_now_ms) > 0);
}

bool workflow_voice_has_pending(void)
{
    return s_queue_len > 0U;
}

void workflow_voice_get_state(workflow_voice_state_t *out)
{
    if (!out) {
        return;
    }

    s_state.supported = bsp_voice_supported();
    s_state.busy = workflow_voice_is_busy();
    s_state.queue_depth = s_queue_len;
    *out = s_state;
}

void workflow_voice_prompt_once(const char *prompt_code, const char *source_code, uint32_t min_gap_ms)
{
    if (!prompt_code || prompt_code[0] == '\0') {
        return;
    }

    if (s_state.last_prompt[0] != '\0' && strcmp(s_state.last_prompt, prompt_code) == 0 &&
        s_state.last_prompt_at_ms != 0U) {
        uint32_t delta_ms = s_now_ms - s_state.last_prompt_at_ms;
        if (delta_ms < min_gap_ms) {
            return;
        }
    }

    workflow_voice_queue_push(prompt_code, source_code);
}

void workflow_voice_prompt_sequence(const char *source_code, const char *const *prompt_codes, size_t count, uint32_t min_gap_ms)
{
    size_t idx;

    if (!prompt_codes || count == 0U) {
        return;
    }

    for (idx = 0U; idx < count; ++idx) {
        workflow_voice_prompt_once(prompt_codes[idx], source_code, idx == 0U ? min_gap_ms : 0U);
    }
}
