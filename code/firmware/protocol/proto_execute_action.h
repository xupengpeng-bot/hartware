#ifndef PROTO_EXECUTE_ACTION_H
#define PROTO_EXECUTE_ACTION_H

#include <stddef.h>

/** Writes COMMAND_ACK or COMMAND_NACK JSON into reply. Returns length or negative. */
int proto_execute_action_handle(const char *json, size_t json_len, char *reply, size_t reply_cap);

#endif /* PROTO_EXECUTE_ACTION_H */
