#ifndef WORKFLOW_VOICE_H
#define WORKFLOW_VOICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WORKFLOW_VOICE_PROMPT_LEN 32U
#define WORKFLOW_VOICE_SOURCE_LEN 32U

typedef struct {
    bool     enabled;
    bool     supported;
    bool     busy;
    uint32_t queue_depth;
    uint32_t last_prompt_at_ms;
    char     last_prompt[WORKFLOW_VOICE_PROMPT_LEN];
    char     last_source[WORKFLOW_VOICE_SOURCE_LEN];
} workflow_voice_state_t;

void workflow_voice_init(void);
void workflow_voice_tick(uint32_t now_ms);
void workflow_voice_clear(void);
bool workflow_voice_is_busy(void);
bool workflow_voice_has_pending(void);
void workflow_voice_get_state(workflow_voice_state_t *out);
void workflow_voice_prompt_once(const char *prompt_code, const char *source_code, uint32_t min_gap_ms);
void workflow_voice_prompt_sequence(const char *source_code, const char *const *prompt_codes, size_t count, uint32_t min_gap_ms);

#endif /* WORKFLOW_VOICE_H */
