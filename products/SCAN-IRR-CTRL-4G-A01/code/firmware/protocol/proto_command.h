#ifndef PROTO_COMMAND_H
#define PROTO_COMMAND_H

#include <stddef.h>
#include <stdint.h>

int proto_build_command_ack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                            const char *command_id, const char *command_code,
                            const char *target_channel_code, const char *accept_state,
                            const char *extra_fields_json);
int proto_build_command_nack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                             const char *command_id, const char *command_code,
                             const char *reject_code, const char *reject_reason,
                             const char *extra_fields_json);

#endif /* PROTO_COMMAND_H */
