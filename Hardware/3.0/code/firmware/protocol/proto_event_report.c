#include "proto_event_report.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include <string.h>

int proto_event_report_build(char *buf, size_t cap, const char *session_ref, const char *event_code,
                             const char *payload_fields_json)
{
    if (!buf || !event_code || cap < 64U) {
        return -1;
    }
    json_buf_t jb;
    json_buf_init(&jb, buf, cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_EVENT_REPORT, 0U, NULL, session_ref) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"event_code\":\"") != 0) {
        return -2;
    }
    if (json_escape_append(&jb, event_code) != 0) {
        return -2;
    }
    if (json_buf_append(&jb, "\"") != 0) {
        return -2;
    }
    if (payload_fields_json && payload_fields_json[0] != '\0') {
        if (json_buf_append(&jb, ",") != 0) {
            return -2;
        }
        if (json_buf_append(&jb, payload_fields_json) != 0) {
            return -2;
        }
    }
    if (proto_envelope_close_payload(&jb) != 0) {
        return -2;
    }
    return (int)jb.len;
}
