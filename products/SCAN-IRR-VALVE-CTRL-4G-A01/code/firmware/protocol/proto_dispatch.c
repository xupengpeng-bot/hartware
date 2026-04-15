#include "proto_dispatch.h"

#include "proto_codec_json.h"
#include "proto_command.h"
#include "proto_envelope.h"

#include <stddef.h>
#include <string.h>

static const proto_dispatch_handlers_t *s_handlers;

static int build_dispatch_nack(const char *json, char *out_reply, size_t out_reply_cap,
                               const char *reject_code, const char *reason)
{
    char corr[48];
    char session_ref[64];

    if (out_reply == NULL || out_reply_cap == 0U) {
        return 1;
    }
    corr[0] = '\0';
    session_ref[0] = '\0';
    (void)proto_json_get_string(json, "c", corr, sizeof(corr));
    (void)proto_json_get_string(json, "r", session_ref, sizeof(session_ref));
    return proto_build_command_nack(out_reply, out_reply_cap,
                                    corr[0] != '\0' ? corr : NULL,
                                    session_ref[0] != '\0' ? session_ref : NULL,
                                    corr[0] != '\0' ? corr : "dispatch",
                                    "INBOUND",
                                    reject_code,
                                    reason,
                                    NULL);
}

void proto_dispatch_init(const proto_dispatch_handlers_t *handlers)
{
    s_handlers = handlers;
}

int proto_dispatch_handle_inbound(const char *json, size_t json_len,
                                  char *out_reply, size_t out_reply_cap, size_t *out_reply_len)
{
    char type_buf[32];
    int r;

    if (out_reply_len != NULL) {
        *out_reply_len = 0U;
    }
    if (json == NULL || json_len == 0U) {
        return -1;
    }
    if (proto_json_get_string(json, "t", type_buf, sizeof(type_buf)) != 0) {
        r = build_dispatch_nack(json, out_reply, out_reply_cap, "PARAM_INVALID", "missing t");
        if (r >= 0 && out_reply_len != NULL) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }
    if (s_handlers == NULL) {
        r = build_dispatch_nack(json, out_reply, out_reply_cap, "UNSUPPORTED_COMMAND", "dispatcher not ready");
        if (r >= 0 && out_reply_len != NULL) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }

    if (strcmp(type_buf, PROTO_MSG_SYNC_CONFIG) == 0 && s_handlers->on_sync_config != NULL && out_reply != NULL) {
        r = s_handlers->on_sync_config(json, json_len, out_reply, out_reply_cap, s_handlers->user);
        if (r >= 0 && out_reply_len != NULL) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }
    if (strcmp(type_buf, PROTO_MSG_QUERY) == 0 && s_handlers->on_query != NULL && out_reply != NULL) {
        r = s_handlers->on_query(json, json_len, out_reply, out_reply_cap, s_handlers->user);
        if (r >= 0 && out_reply_len != NULL) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }
    if (strcmp(type_buf, PROTO_MSG_REGISTER_ACK) == 0 && s_handlers->on_register_ack != NULL) {
        s_handlers->on_register_ack(s_handlers->user);
        return 0;
    }
    if (strcmp(type_buf, PROTO_MSG_REGISTER_NACK) == 0 && s_handlers->on_register_nack != NULL) {
        s_handlers->on_register_nack(s_handlers->user);
        return 0;
    }
    if (strcmp(type_buf, PROTO_MSG_COMMAND_ACK) == 0 && s_handlers->on_command_ack != NULL) {
        s_handlers->on_command_ack(json, json_len, s_handlers->user);
        return 0;
    }
    if (strcmp(type_buf, PROTO_MSG_COMMAND_NACK) == 0 && s_handlers->on_command_nack != NULL) {
        s_handlers->on_command_nack(json, json_len, s_handlers->user);
        return 0;
    }
    if (strcmp(type_buf, PROTO_MSG_QUERY_RESULT) == 0 && s_handlers->on_query_result != NULL) {
        s_handlers->on_query_result(json, json_len, s_handlers->user);
        return 0;
    }
    if (strcmp(type_buf, PROTO_MSG_EXECUTE_ACTION) == 0 && s_handlers->on_execute_action != NULL && out_reply != NULL) {
        r = s_handlers->on_execute_action(json, json_len, out_reply, out_reply_cap, s_handlers->user);
        if (r >= 0 && out_reply_len != NULL) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }

    r = build_dispatch_nack(json, out_reply, out_reply_cap, "UNSUPPORTED_COMMAND", "unsupported t");
    if (r >= 0 && out_reply_len != NULL) {
        *out_reply_len = (size_t)r;
    }
    return r < 0 ? r : 0;
}
