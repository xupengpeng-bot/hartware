#include "proto_envelope.h"
#include "common_identity.h"
#include <stdio.h>
#include <string.h>

static uint32_t s_proto_seq = 0U;

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFFU);
    p[1] = (uint8_t)((v >> 16) & 0xFFU);
    p[2] = (uint8_t)((v >> 8) & 0xFFU);
    p[3] = (uint8_t)(v & 0xFFU);
}

int proto_envelope_decode(const uint8_t *wire, size_t wire_len, proto_envelope_t *out)
{
    if (!wire || !out || wire_len < PROTO_LENGTH_PREFIX_BYTES) {
        return -1;
    }
    uint32_t payload_len = read_be32(wire);
    if (payload_len > (uint32_t)(wire_len - PROTO_LENGTH_PREFIX_BYTES)) {
        return -2;
    }
    out->json_body = (const char *)(wire + PROTO_LENGTH_PREFIX_BYTES);
    out->json_len = (size_t)payload_len;
    out->msg_type[0] = '\0';
    return (int)(PROTO_LENGTH_PREFIX_BYTES + payload_len);
}

int proto_envelope_encode(const char *json_body, size_t json_len, uint8_t *out, size_t out_cap)
{
    if (!json_body || !out) {
        return -1;
    }
    if (json_len > 0xFFFFFFFEU || out_cap < PROTO_LENGTH_PREFIX_BYTES + json_len) {
        return -2;
    }
    write_be32(out, (uint32_t)json_len);
    memcpy(out + PROTO_LENGTH_PREFIX_BYTES, json_body, json_len);
    return (int)(PROTO_LENGTH_PREFIX_BYTES + json_len);
}

uint32_t proto_envelope_take_seq_no(uint32_t seq_no)
{
    if (seq_no != 0U) {
        if (seq_no > s_proto_seq) {
            s_proto_seq = seq_no;
        }
        return seq_no;
    }
    s_proto_seq++;
    if (s_proto_seq == 0U) {
        s_proto_seq = 1U;
    }
    return s_proto_seq;
}

static int append_nullable_string_field(json_buf_t *jb, const char *key, const char *value)
{
    if (!jb || !key) {
        return -1;
    }
    if (json_buf_append(jb, ",\"") != 0) {
        return -1;
    }
    if (json_buf_append(jb, key) != 0) {
        return -1;
    }
    if (json_buf_append(jb, "\":\"") != 0 ||
        json_escape_append(jb, value) != 0 ||
        json_buf_append(jb, "\"") != 0) {
        return -1;
    }
    return 0;
}

int proto_envelope_append_payload_prefix(json_buf_t *jb, const char *msg_type, uint32_t seq_no,
                                         const char *correlation_id, const char *session_ref)
{
    if (!jb || !msg_type) {
        return -1;
    }
    const controller_identity_t *id = common_identity_get();
    const char *imei = (id && id->imei[0] != '\0') ? id->imei : "";
    uint32_t seq = proto_envelope_take_seq_no(seq_no);
    char msg_id[16];

    (void)snprintf(msg_id, sizeof(msg_id), "%06lu", (unsigned long)seq);
    if (json_buf_append(jb, "{\"v\":1,\"t\":\"") != 0) {
        return -1;
    }
    if (json_escape_append(jb, msg_type) != 0) {
        return -1;
    }
    if (json_buf_append(jb, "\",\"i\":\"") != 0) {
        return -1;
    }
    if (json_escape_append(jb, imei) != 0) {
        return -1;
    }
    if (json_buf_append(jb, "\",\"m\":\"") != 0) {
        return -1;
    }
    if (json_escape_append(jb, msg_id) != 0) {
        return -1;
    }
    if (json_buf_append_fmt(jb, "\",\"s\":%lu", (unsigned long)seq) != 0) {
        return -1;
    }
    if (correlation_id != NULL && correlation_id[0] != '\0' &&
        append_nullable_string_field(jb, "c", correlation_id) != 0) {
        return -1;
    }
    if (session_ref != NULL && session_ref[0] != '\0' &&
        append_nullable_string_field(jb, "r", session_ref) != 0) {
        return -1;
    }
    if (json_buf_append(jb, ",\"p\":{") != 0) {
        return -1;
    }
    return 0;
}

int proto_envelope_close_payload(json_buf_t *jb)
{
    if (!jb) {
        return -1;
    }
    return json_buf_append(jb, "}}");
}
