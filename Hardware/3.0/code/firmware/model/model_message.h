/**
 * Logical message / payload shapes — see external device-protocol + message-pack docs.
 * Phase 1: use opaque JSON strings; concrete structs are added when schema pack is wired in.
 */
#ifndef MODEL_MESSAGE_H
#define MODEL_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *json;
    size_t      len;
} model_json_blob_t;

#endif /* MODEL_MESSAGE_H */
