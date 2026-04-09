/**
 * Dispatch inbound JSON to REGISTER / SYNC_CONFIG / QUERY / EXECUTE_ACTION / … — Spec §6.
 */
#ifndef PROTO_DISPATCH_H
#define PROTO_DISPATCH_H

#include <stddef.h>
#include <stdint.h>

typedef struct proto_dispatch_handlers proto_dispatch_handlers_t;

struct proto_dispatch_handlers {
    int (*on_register_build)(char *out_json, size_t out_cap, void *user);
    int (*on_heartbeat_build)(char *out_json, size_t out_cap, void *user);
    int (*on_state_snapshot_build)(char *out_json, size_t out_cap, void *user);
    int (*on_sync_config)(const char *json, size_t len, char *reply_json, size_t reply_cap, void *user);
    int (*on_query)(const char *json, size_t len, char *reply_json, size_t reply_cap, void *user);
    int (*on_execute_action)(const char *json, size_t len, char *reply_json, size_t reply_cap, void *user);
    /** 平台显式确认/拒绝注册（可选；未实现时忽略）。 */
    void (*on_register_ack)(void *user);
    void (*on_register_nack)(void *user);
    void *user;
};

void proto_dispatch_init(const proto_dispatch_handlers_t *handlers);

/**
 * Parse `msg_type` from JSON, invoke handler. Returns 0 handled, 1 unhandled, negative I/O error.
 * `out_reply` optional buffer for synchronous QUERY / EXECUTE_ACTION responses (length-prefixed send is caller's job).
 */
int proto_dispatch_handle_inbound(const char *json, size_t json_len,
                                  char *out_reply, size_t out_reply_cap, size_t *out_reply_len);

#endif /* PROTO_DISPATCH_H */
