#include "proto_json_builder.h"
#include "proto_envelope.h"
#include "common_identity.h"
#include "bsp_rtc.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_ts(char *ts, size_t cap)
{
    if (bsp_rtc_now_iso8601(ts, cap, NULL) == 0) {
        return 0;
    }
    if (cap > 0U) {
        ts[0] = '\0';
    }
    return -1;
}

static int require_nonempty_json_string(const char *json, const char *key)
{
    char pattern[48];
    const char *p;
    size_t klen;
    if (json == NULL || key == NULL) {
        return -1;
    }
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (p == NULL) {
        return -1;
    }
    p = strchr(p, ':');
    if (p == NULL) {
        return -1;
    }
    p++;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    p++;
    klen = 0U;
    while (p[klen] != '\0' && p[klen] != '"') {
        if (p[klen] == '\\' && p[klen + 1U] != '\0') {
            klen += 2U;
            continue;
        }
        klen++;
    }
    return klen == 0U ? -2 : 0;
}

int proto_json_validate_minimal_required(const char *json, size_t len)
{
    size_t start = 0U;
    size_t end = len;
    size_t i;
    int brace_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int escape = 0;

    if (json == NULL || len == 0U) {
        return -1;
    }
    while (start < len && isspace((unsigned char)json[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char)json[end - 1U])) {
        end--;
    }
    if (start >= end || json[start] != '{' || json[end - 1U] != '}') {
        return -2;
    }

    for (i = start; i < end; i++) {
        unsigned char ch = (unsigned char)json[i];
        if (in_string) {
            if (escape) {
                escape = 0;
                continue;
            }
            if (ch == '\\') {
                escape = 1;
                continue;
            }
            if (ch == '"') {
                in_string = 0;
            }
            continue;
        }
        if (ch == '"') {
            in_string = 1;
        } else if (ch == '{') {
            brace_depth++;
        } else if (ch == '}') {
            brace_depth--;
            if (brace_depth < 0) {
                return -3;
            }
        } else if (ch == '[') {
            bracket_depth++;
        } else if (ch == ']') {
            bracket_depth--;
            if (bracket_depth < 0) {
                return -4;
            }
        }
    }
    if (in_string || escape) {
        return -5;
    }
    if (brace_depth != 0 || bracket_depth != 0) {
        return -6;
    }
    if (require_nonempty_json_string(json, "protocol") != 0) {
        return -7;
    }
    if (require_nonempty_json_string(json, "type") != 0) {
        return -8;
    }
    if (require_nonempty_json_string(json, "imei") != 0) {
        return -9;
    }
    if (require_nonempty_json_string(json, "msg_id") != 0) {
        return -10;
    }
    if (strstr(json, "\"seq\"") == NULL) {
        return -11;
    }
    if (strstr(json, "\"payload\"") == NULL) {
        return -12;
    }
    return 0;
}

int proto_json_build_message(char *buf, size_t cap, const char *msg_type, uint32_t seq_no,
                             const char *correlation_id, const char *session_ref, cJSON *payload)
{
    cJSON *root = NULL;
    cJSON *payload_obj = payload;
    const controller_identity_t *id = common_identity_get();
    char msg_id[96];
    char ts[32];
    uint32_t seq;
    int ok;
    int rc = -1;

    if (buf == NULL || cap < 64U || msg_type == NULL || id == NULL || id->imei[0] == '\0') {
        return -1;
    }
    seq = proto_envelope_take_seq_no(seq_no);
    (void)snprintf(msg_id, sizeof(msg_id), "%s-%lu", id->imei, (unsigned long)seq);
    if (build_ts(ts, sizeof(ts)) != 0) {
        return -2;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return -3;
    }
    if (payload_obj == NULL) {
        payload_obj = cJSON_CreateObject();
        if (payload_obj == NULL) {
            cJSON_Delete(root);
            return -4;
        }
    }

    ok = cJSON_AddStringToObject(root, "protocol", PROTO_PROTOCOL_NAME) != NULL &&
         cJSON_AddStringToObject(root, "type", msg_type) != NULL &&
         cJSON_AddStringToObject(root, "imei", id->imei) != NULL &&
         cJSON_AddStringToObject(root, "msg_id", msg_id) != NULL &&
         cJSON_AddNumberToObject(root, "seq", (double)seq) != NULL &&
         cJSON_AddStringToObject(root, "ts", ts) != NULL;
    if (!ok) {
        cJSON_Delete(payload_obj);
        cJSON_Delete(root);
        return -5;
    }
    if (correlation_id != NULL && correlation_id[0] != '\0' &&
        cJSON_AddStringToObject(root, "correlation_id", correlation_id) == NULL) {
        cJSON_Delete(payload_obj);
        cJSON_Delete(root);
        return -6;
    }
    if (session_ref != NULL && session_ref[0] != '\0' &&
        cJSON_AddStringToObject(root, "session_ref", session_ref) == NULL) {
        cJSON_Delete(payload_obj);
        cJSON_Delete(root);
        return -7;
    }
    cJSON_AddItemToObject(root, "payload", payload_obj);
    payload_obj = NULL;

    buf[0] = '\0';
    if (!cJSON_PrintPreallocated(root, buf, (int)cap, 0)) {
        cJSON_Delete(root);
        return -8;
    }
    rc = proto_json_validate_minimal_required(buf, strlen(buf));
    cJSON_Delete(root);
    return rc == 0 ? (int)strlen(buf) : -9;
}

int proto_json_merge_fragment_object(cJSON *dst, const char *fragment_json_fields)
{
    size_t len;
    char *wrapped;
    cJSON *parsed;
    cJSON *child;

    if (dst == NULL) {
        return -1;
    }
    if (fragment_json_fields == NULL || fragment_json_fields[0] == '\0') {
        return 0;
    }
    len = strlen(fragment_json_fields);
    wrapped = (char *)malloc(len + 3U);
    if (wrapped == NULL) {
        return -2;
    }
    wrapped[0] = '{';
    memcpy(wrapped + 1U, fragment_json_fields, len);
    wrapped[len + 1U] = '}';
    wrapped[len + 2U] = '\0';

    parsed = cJSON_Parse(wrapped);
    free(wrapped);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        if (parsed != NULL) {
            cJSON_Delete(parsed);
        }
        return -3;
    }

    child = parsed->child;
    while (child != NULL) {
        cJSON *dup = cJSON_Duplicate(child, 1);
        if (dup == NULL || cJSON_AddItemToObject(dst, child->string, dup) == 0) {
            if (dup != NULL) {
                cJSON_Delete(dup);
            }
            cJSON_Delete(parsed);
            return -4;
        }
        child = child->next;
    }

    cJSON_Delete(parsed);
    return 0;
}
