#include "proto_command.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include <stdio.h>
#include <string.h>

int proto_build_command_ack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                            const char *extra_fields_json)
{
    if (!buf || cap < 64U) {
        return -1;
    }
    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_COMMAND_ACK, 0U, correlation_id, session_ref) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"result\":\"accepted\"") != 0) {
        return -2;
    }
    if (extra_fields_json && extra_fields_json[0] != '\0') {
        if (json_buf_append(&jb, ",") != 0) {
            return -2;
        }
        if (json_buf_append(&jb, extra_fields_json) != 0) {
            return -2;
        }
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}

int proto_build_command_nack(char *buf, size_t cap, const char *correlation_id, const char *session_ref, int32_t code,
                             const char *message)
{
    if (!buf || cap < 64U) {
        return -1;
    }
    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_COMMAND_NACK, 0U, correlation_id, session_ref) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"result\":\"rejected\",\"code\":") != 0) {
        return -2;
    }
    char num[16];
    (void)snprintf(num, sizeof(num), "%ld", (long)code);
    if (json_buf_append(&jb, num) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, ",\"message\":\"") != 0) {
        return -2;
    }
    if (message) {
        if (json_escape_append(&jb, message) != 0) {
            return -2;
        }
    }
    if (json_buf_append(&jb, "\"") != 0) {
        return -2;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}
