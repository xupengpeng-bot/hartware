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
    void (*on_command_ack)(const char *json, size_t len, void *user);
    void (*on_query_result)(const char *json, size_t len, void *user);
    void (*on_register_ack)(void *user);
    void (*on_register_nack)(void *user);
    void *user;
};

void proto_dispatch_init(const proto_dispatch_handlers_t *handlers);
int proto_dispatch_handle_inbound(const char *json, size_t json_len,
                                  char *out_reply, size_t out_reply_cap, size_t *out_reply_len);

#endif /* PROTO_DISPATCH_H */
