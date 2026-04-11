#ifndef WORKFLOW_CARD_READER_H
#define WORKFLOW_CARD_READER_H

#include <stdbool.h>
#include <stdint.h>

#define WORKFLOW_CARD_READER_TOKEN_SUFFIX_LEN 16U
#define WORKFLOW_CARD_READER_REASON_LEN       48U
#define WORKFLOW_CARD_READER_SOURCE_LEN       32U
#define WORKFLOW_CARD_READER_OUTCOME_LEN      32U

typedef struct {
    bool     enabled;
    bool     supported;
    uint32_t uart_port;
    uint32_t rx_buffered_bytes;
    uint32_t audit_pending;
    uint32_t audit_dropped;
    uint32_t frames_ok;
    uint32_t frames_invalid;
    uint32_t reports_sent;
    uint32_t reports_failed;
    uint32_t debounce_dropped;
    uint32_t rejected_count;
    uint32_t last_swipe_at_ms;
    uint32_t last_reported_at_ms;
    char     last_outcome[WORKFLOW_CARD_READER_OUTCOME_LEN];
    char     last_reason[WORKFLOW_CARD_READER_REASON_LEN];
    char     last_source[WORKFLOW_CARD_READER_SOURCE_LEN];
    char     last_token_suffix[WORKFLOW_CARD_READER_TOKEN_SUFFIX_LEN];
} workflow_card_reader_state_t;

void workflow_card_reader_init(void);
void workflow_card_reader_tick(uint32_t now_ms);
void workflow_card_reader_get_state(workflow_card_reader_state_t *out);

#endif /* WORKFLOW_CARD_READER_H */
