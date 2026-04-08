#ifndef PROTO_REGISTER_H
#define PROTO_REGISTER_H

#include <stddef.h>

/** Build REGISTER JSON into buf; returns length or negative. */
int proto_register_build(char *buf, size_t cap);

#endif /* PROTO_REGISTER_H */
