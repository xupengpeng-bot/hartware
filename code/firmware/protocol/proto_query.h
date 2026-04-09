#ifndef PROTO_QUERY_H
#define PROTO_QUERY_H

#include <stddef.h>

/** Build synchronous QUERY reply JSON into reply buffer. Returns length or negative. */
int proto_query_handle(const char *json, size_t json_len, char *reply, size_t reply_cap);

#endif /* PROTO_QUERY_H */
