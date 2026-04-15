#ifndef PROTO_EXECUTE_ACTION_H
#define PROTO_EXECUTE_ACTION_H

#include <stddef.h>
#include <stdint.h>

/** Writes COMMAND_ACK or COMMAND_NACK JSON into reply. Returns length or negative. */
int proto_execute_action_handle(const char *json, size_t json_len, char *reply, size_t reply_cap);
void proto_execute_action_on_reply_send_result(int send_rc, uint32_t monotonic_ms);
void proto_execute_action_poll(uint32_t monotonic_ms);

#endif /* PROTO_EXECUTE_ACTION_H */
