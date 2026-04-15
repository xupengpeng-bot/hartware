#ifndef PROTO_JSON_BUILDER_H
#define PROTO_JSON_BUILDER_H

#include <stddef.h>
#include <stdint.h>

struct cJSON;
typedef struct cJSON cJSON;

int proto_json_validate_minimal_required(const char *json, size_t len);
int proto_json_build_message(char *buf, size_t cap, const char *msg_type, uint32_t seq_no,
                             const char *correlation_id, const char *session_ref, cJSON *payload);
int proto_json_merge_fragment_object(cJSON *dst, const char *fragment_json_fields);

#endif /* PROTO_JSON_BUILDER_H */
