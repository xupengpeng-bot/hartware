#include "proto_command.h"
#include "proto_json_builder.h"
#include "proto_envelope.h"
#include "cJSON.h"

#include <stdio.h>

int proto_build_command_ack(char *buf, size_t cap, const char *correlation_id, const char *session_ref,
                            const char *extra_fields_json)
{
    cJSON *payload;
    int rc;

    if (buf == NULL || cap < 64U) {
        return -1;
    }
    payload = cJSON_CreateObject();
    if (payload == NULL) {
        return -2;
    }
    if (cJSON_AddStringToObject(payload, "result", "accepted") == NULL) {
        cJSON_Delete(payload);
        return -2;
    }
    if (proto_json_merge_fragment_object(payload, extra_fields_json) != 0) {
        cJSON_Delete(payload);
        return -2;
    }
    rc = proto_json_build_message(buf, cap, PROTO_MSG_COMMAND_ACK, 0U, correlation_id, session_ref, payload);
    return rc < 0 ? -2 : rc;
}

int proto_build_command_nack(char *buf, size_t cap, const char *correlation_id, const char *session_ref, int32_t code,
                             const char *message)
{
    cJSON *payload;
    int rc;

    if (buf == NULL || cap < 64U) {
        return -1;
    }
    payload = cJSON_CreateObject();
    if (payload == NULL) {
        return -2;
    }
    if (cJSON_AddStringToObject(payload, "result", "rejected") == NULL ||
        cJSON_AddNumberToObject(payload, "code", (double)code) == NULL ||
        cJSON_AddStringToObject(payload, "message", message != NULL ? message : "") == NULL) {
        cJSON_Delete(payload);
        return -2;
    }
    rc = proto_json_build_message(buf, cap, PROTO_MSG_COMMAND_NACK, 0U, correlation_id, session_ref, payload);
    return rc < 0 ? -2 : rc;
}
