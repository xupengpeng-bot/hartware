/**
 * tcp-json-v1 framing — Firmware Dev Spec v1 §6.1 (4-byte BE length + UTF-8 JSON body).
 * Full schema lives in external device-protocol docs; this layer only handles length + opaque JSON.
 */
#ifndef PROTO_ENVELOPE_H
#define PROTO_ENVELOPE_H

#include <stddef.h>
#include <stdint.h>

#define PROTO_LENGTH_PREFIX_BYTES 4U
#define PROTO_PROTOCOL_NAME       "hj-device-v2"

/** Logical message kinds for dispatch (maps to JSON top-level type field). */
#define PROTO_MSG_REGISTER        "REGISTER"
#define PROTO_MSG_REGISTER_ACK    "REGISTER_ACK"
#define PROTO_MSG_REGISTER_NACK   "REGISTER_NACK"
#define PROTO_MSG_HEARTBEAT       "HEARTBEAT"
#define PROTO_MSG_STATE_SNAPSHOT  "STATE_SNAPSHOT"
#define PROTO_MSG_COMMAND_ACK     "COMMAND_ACK"
#define PROTO_MSG_COMMAND_NACK    "COMMAND_NACK"
#define PROTO_MSG_SYNC_CONFIG     "SYNC_CONFIG"
#define PROTO_MSG_QUERY           "QUERY"
#define PROTO_MSG_QUERY_RESULT    "QUERY_RESULT"
#define PROTO_MSG_EXECUTE_ACTION  "EXECUTE_ACTION"
#define PROTO_MSG_EVENT_REPORT    "EVENT_REPORT"

#include "proto_codec_json.h"

typedef struct {
    char        msg_type[32];
    const char *json_body;
    size_t      json_len;
} proto_envelope_t;

/**
 * Decode one frame from `wire`: first 4 bytes big-endian payload length, then JSON body.
 * Returns bytes consumed on success; negative on error.
 * `out_body` points into `wire` (no copy).
 */
int proto_envelope_decode(const uint8_t *wire, size_t wire_len, proto_envelope_t *out);

/** Write 4-byte BE length followed by body; returns total bytes written or negative. */
int proto_envelope_encode(const char *json_body, size_t json_len, uint8_t *out, size_t out_cap);

/**
 * Append common hj-device-v2 envelope fields and open payload object:
 * {"protocol":"hj-device-v2","type":"...","imei":"...","msg_id":"...","seq":...,"ts":"...","payload":{
 */
int proto_envelope_append_payload_prefix(json_buf_t *jb, const char *msg_type, uint32_t seq_no,
                                         const char *correlation_id, const char *session_ref);

/** Close the payload object and the outer envelope object. */
int proto_envelope_close_payload(json_buf_t *jb);

#endif /* PROTO_ENVELOPE_H */
