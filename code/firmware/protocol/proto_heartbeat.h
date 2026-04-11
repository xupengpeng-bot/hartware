#ifndef PROTO_HEARTBEAT_H
#define PROTO_HEARTBEAT_H

#include <stddef.h>

/** Builds the production V1 heartbeat payload. */
int proto_heartbeat_build(char *buf, size_t cap);

#endif /* PROTO_HEARTBEAT_H */
