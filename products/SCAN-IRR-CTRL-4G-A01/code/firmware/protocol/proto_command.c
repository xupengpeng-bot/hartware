#include "proto_command.h"

#include "proto_codec_json.h"
#include "proto_envelope.h"

static int proto_build_command_common(char *buf, size_t cap, const char *msg_type,
                                      const char *correlation_id, const char *session_ref,
                                      const char *fields_json)
{
    json_buf_t jb;

    if (buf == NULL || cap < 128U || msg_type == NULL || fields_json == NULL) {
        return -1;
    }
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, msg_type, 0U, correlation_id, session_ref) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, fields_json) != 0) {
        return -3;
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -4;
    }
    return (int)jb.len;
}

int proto_build_command_ack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                            const char *command_id, const char *command_code,
                            const char *target_channel_code, const char *accept_state,
                            const char *extra_fields_json)
{
    json_buf_t fields;
    char field_buf[384];

    if (command_id == NULL || command_code == NULL || target_channel_code == NULL || accept_state == NULL) {
        return -1;
    }
    json_buf_init(&fields, field_buf, sizeof(field_buf));
    (void)command_id;
    (void)command_code;
    (void)accept_state;
    if (json_buf_append(&fields, "\"tr\":\"") != 0 ||
        json_escape_append(&fields, target_channel_code) != 0 ||
        json_buf_append(&fields, "\"") != 0) {
        return -1;
    }
    if (extra_fields_json != NULL && extra_fields_json[0] != '\0') {
        if (json_buf_append(&fields, ",") != 0 ||
            json_buf_append(&fields, extra_fields_json) != 0) {
            return -1;
        }
    }
    return proto_build_command_common(buf, cap, PROTO_MSG_COMMAND_ACK, correlation_id, session_ref, field_buf);
}

int proto_build_command_nack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                             const char *command_id, const char *command_code,
                             const char *reject_code, const char *reject_reason,
                             const char *extra_fields_json)
{
    json_buf_t fields;
    char field_buf[448];

    if (command_id == NULL || command_code == NULL || reject_code == NULL || reject_reason == NULL) {
        return -1;
    }
    json_buf_init(&fields, field_buf, sizeof(field_buf));
    (void)command_id;
    (void)command_code;
    if (json_buf_append(&fields, "\"rc\":\"") != 0 ||
        json_escape_append(&fields, proto_map_reject_short(reject_code)) != 0 ||
        json_buf_append(&fields, "\",\"msg\":\"") != 0 ||
        json_escape_append(&fields, reject_reason) != 0 ||
        json_buf_append(&fields, "\"") != 0) {
        return -1;
    }
    if (extra_fields_json != NULL && extra_fields_json[0] != '\0') {
        if (json_buf_append(&fields, ",") != 0 ||
            json_buf_append(&fields, extra_fields_json) != 0) {
            return -1;
        }
    }
    return proto_build_command_common(buf, cap, PROTO_MSG_COMMAND_NACK, correlation_id, session_ref, field_buf);
}
