#ifndef PROTO_COMMAND_H
#define PROTO_COMMAND_H

#include <stddef.h>
#include <stdint.h>

int proto_build_command_ack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                            const char *extra_fields_json);
int proto_build_command_nack(char *buf, size_t cap, const char *correlation_id, const char *session_ref, int32_t code,
                             const char *message);

#endif /* PROTO_COMMAND_H */
