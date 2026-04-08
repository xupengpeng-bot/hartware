#ifndef PROTO_SYNC_CONFIG_H
#define PROTO_SYNC_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/**
 * Apply SYNC_CONFIG JSON: validate, stage inactive buffer, swap active.
 * Returns 0 on success, negative on error. Optional `ack_json` COMMAND_ACK body.
 */
int proto_sync_config_apply(const char *json, size_t json_len, char *ack_json, size_t ack_cap);

#endif /* PROTO_SYNC_CONFIG_H */
