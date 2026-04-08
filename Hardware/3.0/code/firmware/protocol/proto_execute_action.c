#include "proto_execute_action.h"
#include "proto_codec_json.h"
#include "proto_command.h"
#include "workflow_control.h"
#include "workflow_local_access.h"
#include "workflow_voice.h"
#include "module_registry.h"

#include "proto_ota.h"
#include "ota_types.h"

#include <string.h>
#include <stdio.h>

static int str_eq(const char *a, const char *b)
{
    if (!a || !b) {
        return 0;
    }
    return strcmp(a, b) == 0;
}

static const char *local_access_decision_label(workflow_local_access_decision_t decision)
{
    switch (decision) {
    case WORKFLOW_LOCAL_ACCESS_ALLOW_START:
        return "start_session";
    case WORKFLOW_LOCAL_ACCESS_ALLOW_STOP:
        return "stop_session";
    case WORKFLOW_LOCAL_ACCESS_REJECT_DEBOUNCE:
        return "rejected_debounce";
    case WORKFLOW_LOCAL_ACCESS_REJECT_NOT_READY:
        return "rejected_not_ready";
    case WORKFLOW_LOCAL_ACCESS_REJECT_BUSY:
        return "rejected_busy";
    case WORKFLOW_LOCAL_ACCESS_REJECT_STOP_GUARD:
        return "rejected_stop_guard";
    default:
        return "rejected_invalid";
    }
}

static int map_ota_action(const char *code, ota_action_code_t *out)
{
    if (str_eq(code, "ota_prepare")) {
        *out = OTA_ACTION_PREPARE;
        return 0;
    }
    if (str_eq(code, "ota_start")) {
        *out = OTA_ACTION_START;
        return 0;
    }
    if (str_eq(code, "ota_cancel")) {
        *out = OTA_ACTION_CANCEL;
        return 0;
    }
    if (str_eq(code, "ota_commit")) {
        *out = OTA_ACTION_COMMIT;
        return 0;
    }
    if (str_eq(code, "ota_rollback")) {
        *out = OTA_ACTION_ROLLBACK;
        return 0;
    }
    return -1;
}

static int fill_ota_prepare_from_json(const char *json, ota_prepare_payload_t *p)
{
    memset(p, 0, sizeof(*p));
    (void)proto_json_get_string(json, "target_version", p->target_version, sizeof(p->target_version));
    (void)proto_json_get_string(json, "package_url", p->package_url, sizeof(p->package_url));
    {
        uint32_t sz = 0U;
        (void)proto_json_get_u32(json, "package_size", &sz);
        p->package_size = sz;
    }
    (void)proto_json_get_string(json, "package_sha256", p->package_sha256_hex, sizeof(p->package_sha256_hex));
    (void)proto_json_get_string(json, "package_format", p->package_format, sizeof(p->package_format));
    {
        uint32_t mb = 0U;
        if (proto_json_get_u32(json, "min_battery_soc", &mb) == 0) {
            p->min_battery_soc = (uint8_t)(mb > 255U ? 255U : mb);
        }
    }
    {
        int32_t csq = 0;
        if (proto_json_get_i32(json, "min_signal_csq", &csq) == 0) {
            p->min_signal_csq = (int16_t)csq;
        }
    }
    {
        uint32_t force_upgrade = 0U;
        if (proto_json_get_u32(json, "force_upgrade", &force_upgrade) == 0) {
            p->force_upgrade = force_upgrade != 0U;
        }
    }
    {
        uint32_t allow_running_upgrade = 0U;
        if (proto_json_get_u32(json, "allow_running_upgrade", &allow_running_upgrade) == 0) {
            p->allow_running_upgrade = allow_running_upgrade != 0U;
        }
    }
    {
        uint32_t auto_commit = 0U;
        if (proto_json_get_u32(json, "auto_commit", &auto_commit) == 0) {
            p->auto_commit = auto_commit != 0U;
        }
    }
    (void)proto_json_get_string(json, "upgrade_ticket", p->upgrade_ticket, sizeof(p->upgrade_ticket));
    return 0;
}

int proto_execute_action_handle(const char *json, size_t json_len, char *reply, size_t reply_cap)
{
    char corr[48];
    char session_ref[64];
    char scope[24];
    char action[48];

    (void)json_len;
    if (!json || !reply || reply_cap < 128U) {
        return -1;
    }

    corr[0] = '\0';
    session_ref[0] = '\0';
    (void)proto_json_get_string(json, "correlation_id", corr, sizeof(corr));
    (void)proto_json_get_string(json, "session_ref", session_ref, sizeof(session_ref));
    if (session_ref[0] == '\0') {
        (void)proto_json_get_string(json, "session_id", session_ref, sizeof(session_ref));
    }
    if (proto_json_get_string(json, "scope", scope, sizeof(scope)) != 0) {
        return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -1, "missing scope");
    }
    if (proto_json_get_string(json, "action_code", action, sizeof(action)) != 0) {
        return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -2, "missing action_code");
    }

    if (str_eq(scope, "workflow")) {
        if (str_eq(action, "start_session")) {
            char                       resolved_session_ref[MODEL_SESSION_ID_LEN];
            bool                       idempotent = false;
            int                        result;
            char                       extra[256];

            memset(resolved_session_ref, 0, sizeof(resolved_session_ref));
            result = workflow_control_start_session(session_ref[0] ? session_ref : NULL, resolved_session_ref,
                                                    sizeof(resolved_session_ref), &idempotent);
            if (result < 0) {
                return proto_build_command_nack(reply, reply_cap, corr,
                                               resolved_session_ref[0] ? resolved_session_ref : (session_ref[0] ? session_ref : NULL),
                                               result, workflow_control_error_message(result));
            }
            (void)snprintf(extra, sizeof(extra),
                           "\"command_code\":\"START_SESSION\",\"action_code\":\"start_session\","
                           "\"workflow_state\":\"%s\",\"idempotent\":%s",
                           workflow_control_state_label(workflow_engine_get_state()),
                           idempotent ? "true" : "false");
            return proto_build_command_ack(reply, reply_cap, corr, resolved_session_ref, extra);
        }

        if (str_eq(action, "stop_session")) {
            bool idempotent = false;
            int  result = workflow_control_stop_session(session_ref[0] ? session_ref : NULL, &idempotent);
            char extra[256];

            if (result < 0) {
                return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, result,
                                               workflow_control_error_message(result));
            }
            (void)snprintf(extra, sizeof(extra),
                           "\"command_code\":\"STOP_SESSION\",\"action_code\":\"stop_session\","
                           "\"workflow_state\":\"%s\",\"idempotent\":%s",
                           workflow_control_state_label(workflow_engine_get_state()),
                           idempotent ? "true" : "false");
            return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, extra);
        }

        if (str_eq(action, "pause_session")) {
            int result = workflow_engine_request_pause_session();
            if (result != WORKFLOW_REQ_OK) {
                return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, result,
                                               workflow_control_error_message(result));
            }
            workflow_voice_prompt_once("irrigation_paused", "workflow_pause", 1000U);
            return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL,
                                           "\"action_code\":\"pause_session\",\"workflow_state\":\"paused\"");
        }

        if (str_eq(action, "resume_session")) {
            int result = workflow_engine_request_resume_session();
            if (result != WORKFLOW_REQ_OK) {
                return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, result,
                                               workflow_control_error_message(result));
            }
            workflow_voice_prompt_once("irrigation_resumed", "workflow_resume", 1000U);
            return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL,
                                           "\"action_code\":\"resume_session\",\"workflow_state\":\"running\"");
        }

        if (str_eq(action, "local_access_submit")) {
            char                             access_token[WORKFLOW_LOCAL_ACCESS_TOKEN_LEN];
            char                             access_source[WORKFLOW_LOCAL_ACCESS_SOURCE_LEN];
            char                             access_reason[WORKFLOW_LOCAL_ACCESS_REASON_LEN];
            char                             access_session_ref[MODEL_SESSION_ID_LEN];
            bool                             idempotent = false;
            workflow_local_access_decision_t decision;
            char                             extra[320];

            if (proto_json_get_string(json, "access_token", access_token, sizeof(access_token)) != 0) {
                return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -14,
                                               "missing access_token");
            }
            access_source[0] = '\0';
            access_reason[0] = '\0';
            access_session_ref[0] = '\0';
            (void)proto_json_get_string(json, "access_source", access_source, sizeof(access_source));

            decision = workflow_local_access_submit_token(access_token, access_source, access_reason,
                                                          sizeof(access_reason), access_session_ref,
                                                          sizeof(access_session_ref), &idempotent);
            if (decision < 0) {
                return proto_build_command_nack(reply, reply_cap, corr,
                                               access_session_ref[0] ? access_session_ref : (session_ref[0] ? session_ref : NULL),
                                               decision, access_reason[0] ? access_reason : "local access rejected");
            }

            (void)snprintf(extra, sizeof(extra),
                           "\"action_code\":\"local_access_submit\","
                           "\"access_decision\":\"%s\","
                           "\"access_reason\":\"%s\","
                           "\"workflow_state\":\"%s\","
                           "\"idempotent\":%s",
                           local_access_decision_label(decision),
                           access_reason,
                           workflow_control_state_label(workflow_engine_get_state()),
                           idempotent ? "true" : "false");
            return proto_build_command_ack(reply, reply_cap, corr,
                                           access_session_ref[0] ? access_session_ref : (session_ref[0] ? session_ref : NULL),
                                           extra);
        }

        return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -9,
                                       "unknown workflow action");
    }

    if (str_eq(scope, "common")) {
        if (str_eq(action, "play_voice_prompt")) {
            char     prompt_code[WORKFLOW_VOICE_PROMPT_LEN];
            char     prompt_source[WORKFLOW_VOICE_SOURCE_LEN];
            uint32_t min_gap_ms = 0U;
            uint32_t clear_queue = 0U;
            char     extra[192];

            if (proto_json_get_string(json, "prompt_code", prompt_code, sizeof(prompt_code)) != 0) {
                return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -15,
                                               "missing prompt_code");
            }
            prompt_source[0] = '\0';
            (void)proto_json_get_string(json, "prompt_source", prompt_source, sizeof(prompt_source));
            (void)proto_json_get_u32(json, "min_gap_ms", &min_gap_ms);
            (void)proto_json_get_u32(json, "clear_queue", &clear_queue);

            if (clear_queue != 0U) {
                workflow_voice_clear();
            }
            workflow_voice_prompt_once(prompt_code, prompt_source[0] != '\0' ? prompt_source : "platform_voice",
                                       min_gap_ms);
            (void)snprintf(extra, sizeof(extra),
                           "\"action_code\":\"play_voice_prompt\",\"prompt_code\":\"%s\"",
                           prompt_code);
            return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, extra);
        }

        ota_action_code_t oa = OTA_ACTION_PREPARE;
        if (map_ota_action(action, &oa) == 0) {
            ota_prepare_payload_t pay;
            const ota_prepare_payload_t *prepare_payload;
            int                         result;
            char                        extra[192];

            fill_ota_prepare_from_json(json, &pay);
            prepare_payload = (oa == OTA_ACTION_PREPARE) ? &pay : NULL;
            result = proto_ota_execute_action(oa, prepare_payload);
            if (result != 0) {
                return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, result,
                                               "ota rejected");
            }
            (void)snprintf(extra, sizeof(extra), "\"action_code\":\"%s\",\"ota\":\"accepted\"", action);
            return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, extra);
        }
        return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -10,
                                       "unknown common action");
    }

    if (str_eq(scope, "module")) {
        char                module_code[48];
        char                target_ref[48];
        const module_ops_t *module;
        uint8_t             exec_result;
        char                extra[192];

        if (proto_json_get_string(json, "module_code", module_code, sizeof(module_code)) != 0) {
            return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -11,
                                           "missing module_code");
        }

        module = module_registry_get(module_code);
        if (!module || !module->execute_action) {
            return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -12,
                                           "module not found");
        }

        target_ref[0] = '\0';
        (void)proto_json_get_string(json, "target_ref", target_ref, sizeof(target_ref));
        exec_result = module->execute_action(action, target_ref[0] ? target_ref : NULL, NULL);
        if (exec_result != 0U) {
            return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -13,
                                           "module action failed");
        }

        (void)snprintf(extra, sizeof(extra),
                       "\"action_code\":\"%s\",\"module_code\":\"%s\",\"module\":\"ok\"",
                       action, module_code);
        return proto_build_command_ack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, extra);
    }

    return proto_build_command_nack(reply, reply_cap, corr, session_ref[0] ? session_ref : NULL, -99,
                                   "unsupported scope");
}
