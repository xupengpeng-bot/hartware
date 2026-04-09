#include "proto_dispatch.h"
#include "proto_envelope.h"
#include "proto_codec_json.h"

#include <stddef.h>
#include <string.h>

static const proto_dispatch_handlers_t *s_handlers;

void proto_dispatch_init(const proto_dispatch_handlers_t *handlers)
{
    s_handlers = handlers;
}

int proto_dispatch_handle_inbound(const char *json, size_t json_len,
                                  char *out_reply, size_t out_reply_cap, size_t *out_reply_len)
{
    if (out_reply_len) {
        *out_reply_len = 0U;
    }
    if (!json || json_len == 0U) {
        return -1;
    }
    char type_buf[32];
    if (proto_json_get_string(json, "type", type_buf, sizeof(type_buf)) != 0) {
        return 1;
    }

    if (s_handlers == NULL) {
        return 1;
    }

    if (strcmp(type_buf, PROTO_MSG_SYNC_CONFIG) == 0 && s_handlers->on_sync_config && out_reply) {
        int r = s_handlers->on_sync_config(json, json_len, out_reply, out_reply_cap, s_handlers->user);
        if (r >= 0 && out_reply_len) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }
    if (strcmp(type_buf, PROTO_MSG_QUERY) == 0 && s_handlers->on_query && out_reply) {
        int r = s_handlers->on_query(json, json_len, out_reply, out_reply_cap, s_handlers->user);
        if (r >= 0 && out_reply_len) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }
    if (strcmp(type_buf, PROTO_MSG_EXECUTE_ACTION) == 0 && s_handlers->on_execute_action && out_reply) {
        int r = s_handlers->on_execute_action(json, json_len, out_reply, out_reply_cap, s_handlers->user);
        if (r >= 0 && out_reply_len) {
            *out_reply_len = (size_t)r;
        }
        return r < 0 ? r : 0;
    }
    if (strcmp(type_buf, PROTO_MSG_REGISTER_ACK) == 0) {
        if (s_handlers->on_register_ack) {
            s_handlers->on_register_ack(s_handlers->user);
        }
        return 0;
    }
    if (strcmp(type_buf, PROTO_MSG_REGISTER_NACK) == 0) {
        if (s_handlers->on_register_nack) {
            s_handlers->on_register_nack(s_handlers->user);
        }
        return 0;
    }

    (void)json_len;
    return 1;
}
