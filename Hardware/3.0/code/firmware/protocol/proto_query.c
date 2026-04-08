#include "proto_query.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "common_status.h"
#include "workflow_engine.h"
#include "workflow_card_reader.h"
#include "workflow_local_access.h"
#include "workflow_voice.h"
#include "module_registry.h"
#include "module_pressure.h"
#include "module_flow.h"
#include "module_meter.h"
#include "module_soil_moisture.h"
#include "module_soil_temperature.h"

#include "proto_ota.h"

#include <stdio.h>
#include <string.h>

static const char *ota_state_str(ota_state_t st)
{
    switch (st) {
    case OTA_STATE_IDLE:
        return "IDLE";
    case OTA_STATE_PRECHECKING:
        return "PRECHECKING";
    case OTA_STATE_PRECHECK_FAILED:
        return "PRECHECK_FAILED";
    case OTA_STATE_READY_TO_DOWNLOAD:
        return "READY_TO_DOWNLOAD";
    case OTA_STATE_DOWNLOADING:
        return "DOWNLOADING";
    case OTA_STATE_DOWNLOAD_FAILED:
        return "DOWNLOAD_FAILED";
    case OTA_STATE_DOWNLOADED:
        return "DOWNLOADED";
    case OTA_STATE_VERIFYING:
        return "VERIFYING";
    case OTA_STATE_VERIFY_FAILED:
        return "VERIFY_FAILED";
    case OTA_STATE_VERIFIED:
        return "VERIFIED";
    case OTA_STATE_WRITING:
        return "WRITING";
    case OTA_STATE_WRITE_FAILED:
        return "WRITE_FAILED";
    case OTA_STATE_READY_TO_SWITCH:
        return "READY_TO_SWITCH";
    case OTA_STATE_SWITCHING:
        return "SWITCHING";
    case OTA_STATE_UPGRADED:
        return "UPGRADED";
    case OTA_STATE_UPGRADE_FAILED:
        return "UPGRADE_FAILED";
    case OTA_STATE_ROLLING_BACK:
        return "ROLLING_BACK";
    case OTA_STATE_ROLLED_BACK:
        return "ROLLED_BACK";
    default:
        return "UNKNOWN";
    }
}

static const char *workflow_state_str(workflow_state_t st)
{
    switch (st) {
    case WF_BOOTING:
        return "booting";
    case WF_ONLINE_NOT_READY:
        return "online_not_ready";
    case WF_READY_IDLE:
        return "ready_idle";
    case WF_STARTING:
        return "starting";
    case WF_RUNNING:
        return "running";
    case WF_PAUSING:
        return "pausing";
    case WF_PAUSED:
        return "paused";
    case WF_RESUMING:
        return "resuming";
    case WF_STOPPING:
        return "stopping";
    case WF_STOPPED:
        return "stopped";
    case WF_ERROR_STOP:
        return "error_stop";
    default:
        return "unknown";
    }
}

int proto_query_handle(const char *json, size_t json_len, char *reply, size_t reply_cap)
{
    (void)json_len;
    if (!json || !reply || reply_cap < 64U) {
        return -1;
    }
    char corr[48];
    char session_ref[64];
    char scope[24];
    char qcode[48];
    corr[0] = '\0';
    session_ref[0] = '\0';
    (void)proto_json_get_string(json, "correlation_id", corr, sizeof(corr));
    (void)proto_json_get_string(json, "session_ref", session_ref, sizeof(session_ref));
    if (session_ref[0] == '\0') {
        (void)proto_json_get_string(json, "session_id", session_ref, sizeof(session_ref));
    }
    if (proto_json_get_string(json, "scope", scope, sizeof(scope)) != 0) {
        return -2;
    }
    if (proto_json_get_string(json, "query_code", qcode, sizeof(qcode)) != 0) {
        return -3;
    }

    json_buf_t jb;
    json_buf_init(&jb, reply, reply_cap);
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY_RESULT, 0U, corr, session_ref[0] ? session_ref : NULL) != 0) {
        return -4;
    }
    if (json_buf_append(&jb, "\"scope\":\"") != 0) {
        return -4;
    }
    if (json_escape_append(&jb, scope) != 0) {
        return -4;
    }
    if (json_buf_append(&jb, "\",\"query_code\":\"") != 0) {
        return -4;
    }
    if (json_escape_append(&jb, qcode) != 0) {
        return -4;
    }

    if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_workflow_state") == 0) {
        workflow_state_t       st = workflow_engine_get_state();
        device_runtime_t      *rt = workflow_engine_runtime();
        uint32_t               guard_remaining_ms = workflow_engine_stop_guard_remaining_ms();
        const char            *active_session_id = (rt && rt->active_session.session_id[0] != '\0')
                                                     ? rt->active_session.session_id
                                                     : "";
        const char            *last_session_id = (rt && rt->recovery.last_session_id[0] != '\0')
                                                   ? rt->recovery.last_session_id
                                                   : "";
        char tmp[512];
        (void)snprintf(
            tmp,
            sizeof(tmp),
            "\",\"controller_state\":{"
            "\"workflow_state\":\"%s\","
            "\"active_session_id\":\"%s\","
            "\"active_session_started_at_utc\":%lu,"
            "\"recovery_pending\":%s,"
            "\"settlement_pending\":%s,"
            "\"stop_guard_remaining_ms\":%lu,"
            "\"last_stop_reason_code\":%lu,"
            "\"last_stop_at_utc\":%lu,"
            "\"last_session_id\":\"%s\","
            "\"last_session_started_at_utc\":%lu,"
            "\"last_recovery_hint\":\"%s\""
            "}",
            workflow_state_str(st),
            active_session_id,
            (unsigned long)(rt ? rt->active_session.started_at_utc : 0U),
            (rt && rt->recovery.recovery_pending) ? "true" : "false",
            (rt && rt->recovery.settlement_pending) ? "true" : "false",
            (unsigned long)guard_remaining_ms,
            (unsigned long)(rt ? rt->recovery.last_stop_reason_code : 0U),
            (unsigned long)(rt ? rt->recovery.last_stop_at_utc : 0U),
            last_session_id,
            (unsigned long)(rt ? rt->recovery.last_session_started_at_utc : 0U),
            (rt && rt->recovery.last_recovery_hint[0] != '\0') ? rt->recovery.last_recovery_hint : "");
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_local_access_policy") == 0) {
        workflow_local_access_policy_t policy;
        char                           tmp[256];

        workflow_local_access_get_policy(&policy);
        (void)snprintf(
            tmp,
            sizeof(tmp),
            "\",\"local_access\":{"
            "\"mode\":\"card_or_local_token\","
            "\"global_debounce_ms\":%lu,"
            "\"same_token_debounce_ms\":%lu,"
            "\"post_stop_drop_window_ms\":%lu,"
            "\"active_token_bound\":%s"
            "}",
            (unsigned long)policy.global_debounce_ms,
            (unsigned long)policy.same_token_debounce_ms,
            (unsigned long)policy.post_stop_drop_window_ms,
            workflow_local_access_has_active_token() ? "true" : "false");
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_local_access_state") == 0) {
        workflow_local_access_state_t state;
        char                          tmp[384];

        workflow_local_access_get_state(&state);
        (void)snprintf(
            tmp,
            sizeof(tmp),
            "\",\"local_access_state\":{"
            "\"last_outcome\":\"%s\","
            "\"last_reason\":\"%s\","
            "\"last_source\":\"%s\","
            "\"last_session_id\":\"%s\","
            "\"last_seen_at_ms\":%lu,"
            "\"last_decision_at_ms\":%lu,"
            "\"last_idempotent\":%s,"
            "\"active_token_bound\":%s"
            "}",
            workflow_local_access_outcome_label(state.last_outcome),
            state.last_reason,
            state.last_source,
            state.last_session_id,
            (unsigned long)state.last_seen_at_ms,
            (unsigned long)state.last_decision_at_ms,
            state.last_idempotent ? "true" : "false",
            state.active_token_bound ? "true" : "false");
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_card_reader_state") == 0) {
        workflow_card_reader_state_t state;
        char                         tmp[640];

        workflow_card_reader_get_state(&state);
        (void)snprintf(
            tmp,
            sizeof(tmp),
            "\",\"card_reader_state\":{"
            "\"mode\":\"platform_checkout_card_reader\","
            "\"enabled\":%s,"
            "\"supported\":%s,"
            "\"uart_port\":%lu,"
            "\"rx_buffered_bytes\":%lu,"
            "\"frames_ok\":%lu,"
            "\"frames_invalid\":%lu,"
            "\"reports_sent\":%lu,"
            "\"reports_failed\":%lu,"
            "\"debounce_dropped\":%lu,"
            "\"rejected_count\":%lu,"
            "\"last_outcome\":\"%s\","
            "\"last_reason\":\"%s\","
            "\"last_source\":\"%s\","
            "\"last_token_suffix\":\"%s\","
            "\"last_swipe_at_ms\":%lu,"
            "\"last_reported_at_ms\":%lu"
            "}",
            state.enabled ? "true" : "false",
            state.supported ? "true" : "false",
            (unsigned long)state.uart_port,
            (unsigned long)state.rx_buffered_bytes,
            (unsigned long)state.frames_ok,
            (unsigned long)state.frames_invalid,
            (unsigned long)state.reports_sent,
            (unsigned long)state.reports_failed,
            (unsigned long)state.debounce_dropped,
            (unsigned long)state.rejected_count,
            state.last_outcome,
            state.last_reason,
            state.last_source,
            state.last_token_suffix,
            (unsigned long)state.last_swipe_at_ms,
            (unsigned long)state.last_reported_at_ms);
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "workflow") == 0 && strcmp(qcode, "query_voice_state") == 0) {
        workflow_voice_state_t state;
        char                   tmp[320];

        workflow_voice_get_state(&state);
        (void)snprintf(
            tmp,
            sizeof(tmp),
            "\",\"voice_state\":{"
            "\"enabled\":%s,"
            "\"supported\":%s,"
            "\"busy\":%s,"
            "\"queue_depth\":%lu,"
            "\"last_prompt\":\"%s\","
            "\"last_source\":\"%s\","
            "\"last_prompt_at_ms\":%lu"
            "}",
            state.enabled ? "true" : "false",
            state.supported ? "true" : "false",
            state.busy ? "true" : "false",
            (unsigned long)state.queue_depth,
            state.last_prompt,
            state.last_source,
            (unsigned long)state.last_prompt_at_ms);
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "common") == 0 && strcmp(qcode, "query_upgrade_status") == 0) {
        ota_upgrade_status_t s;
        memset(&s, 0, sizeof(s));
        (void)proto_ota_query_upgrade_status(&s);
        char tmp[384];
        (void)snprintf(tmp, sizeof(tmp),
                       "\",\"upgrade_status\":{"
                       "\"ota_state\":\"%s\","
                       "\"target_version\":\"%s\","
                       "\"current_version\":\"%s\","
                       "\"package_sha256\":\"%s\","
                       "\"download_progress_pct\":%u,"
                       "\"write_progress_pct\":%u,"
                       "\"last_result\":%u,"
                       "\"last_error_code\":%ld,"
                       "\"last_error_message\":\"%s\""
                       "}",
                       ota_state_str(s.ota_state), s.target_version, s.current_version, s.package_sha256_hex,
                       (unsigned)s.download_progress_pct, (unsigned)s.write_progress_pct,
                       (unsigned)s.last_result, (long)s.last_error_code, s.last_error_message);
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "common") == 0 && strcmp(qcode, "query_upgrade_capability") == 0) {
        ota_upgrade_capability_t c;
        memset(&c, 0, sizeof(c));
        (void)proto_ota_query_upgrade_capability(&c);
        char tmp[256];
        (void)snprintf(tmp, sizeof(tmp),
                       "\",\"upgrade_capability\":{"
                       "\"ota_supported\":%s,"
                       "\"dual_bank\":%s,"
                       "\"package_formats\":\"%s\","
                       "\"compression_formats\":\"%s\","
                       "\"min_battery_soc_default\":%u,"
                       "\"min_signal_csq_default\":%d"
                       "}",
                       c.ota_supported ? "true" : "false", c.dual_bank ? "true" : "false",
                       c.package_formats, c.compression_formats,
                       (unsigned)c.min_battery_soc_default, (int)c.min_signal_csq_default);
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "common") == 0 && strcmp(qcode, "query_common_status") == 0) {
        const common_status_t *cs = common_status_get();
        char tmp[320];
        (void)snprintf(tmp, sizeof(tmp),
                       "\",\"common_status\":{"
                       "\"online\":%s,\"ready\":%s,\"signal_csq\":%d,\"battery_soc\":%u,"
                       "\"config_version\":%lu"
                       "}",
                       cs->online ? "true" : "false", cs->ready ? "true" : "false",
                       (int)cs->signal_csq, (unsigned)cs->battery_soc,
                       (unsigned long)cs->config_version);
        if (json_buf_append(&jb, tmp) != 0) {
            return -4;
        }
    } else if (strcmp(scope, "module") == 0) {
        char mcode[48];
        if (proto_json_get_string(json, "module_code", mcode, sizeof(mcode)) != 0) {
            if (json_buf_append(&jb, "\",\"error\":\"missing module_code\"") != 0) {
                return -4;
            }
        } else {
            const module_ops_t *m = module_registry_get(mcode);
            if (!m) {
                if (json_buf_append(&jb, "\",\"module_code\":\"") != 0) {
                    return -4;
                }
                if (json_escape_append(&jb, mcode) != 0) {
                    return -4;
                }
                if (json_buf_append(&jb, "\",\"error\":\"unknown module\"") != 0) {
                    return -4;
                }
            } else {
                if (strcmp(mcode, "pressure_acquisition") == 0) {
                    module_pressure_values_t v;
                    memset(&v, 0, sizeof(v));
                    (void)m->query_values(&v);
                    char tmp[256];
                    (void)snprintf(tmp, sizeof(tmp),
                                   "\",\"module_code\":\"%s\",\"channel_values\":[{"
                                   "\"module_code\":\"%s\",\"channel_code\":\"pressure_1\",\"metric_code\":\"pressure_mpa\","
                                   "\"value\":%.4f,\"unit\":\"MPa\",\"quality\":\"%s\""
                                   "}]", mcode, mcode, (double)v.pressure_mpa,
                                   v.quality != 0U ? "good" : "bad");
                    if (json_buf_append(&jb, tmp) != 0) {
                        return -4;
                    }
                } else if (strcmp(mcode, "flow_acquisition") == 0) {
                    module_flow_values_t v;
                    memset(&v, 0, sizeof(v));
                    (void)m->query_values(&v);
                    char tmp[384];
                    (void)snprintf(tmp, sizeof(tmp),
                                   "\",\"module_code\":\"%s\",\"channel_values\":["
                                   "{"
                                   "\"module_code\":\"%s\",\"channel_code\":\"flow_1\",\"metric_code\":\"flow_m3h\","
                                   "\"value\":%.4f,\"unit\":\"m3/h\",\"quality\":\"good\""
                                   "},"
                                   "{"
                                   "\"module_code\":\"%s\",\"channel_code\":\"flow_total\",\"metric_code\":\"total_m3\","
                                   "\"value\":%.3f,\"unit\":\"m3\",\"quality\":\"good\""
                                   "}"
                                   "]",
                                   mcode, mcode, (double)v.instant_m3h, mcode, v.total_m3);
                    if (json_buf_append(&jb, tmp) != 0) {
                        return -4;
                    }
                } else if (strcmp(mcode, "electric_meter_modbus") == 0) {
                    module_meter_values_t v;
                    memset(&v, 0, sizeof(v));
                    (void)m->query_values(&v);
                    char tmp[768];
                    (void)snprintf(tmp, sizeof(tmp),
                                   "\",\"module_code\":\"%s\",\"channel_values\":["
                                   "{\"module_code\":\"%s\",\"channel_code\":\"meter_energy\",\"metric_code\":\"energy_kwh\",\"value\":%.3f,\"unit\":\"kWh\",\"quality\":\"good\"},"
                                   "{\"module_code\":\"%s\",\"channel_code\":\"meter_power\",\"metric_code\":\"power_kw\",\"value\":%.3f,\"unit\":\"kW\",\"quality\":\"good\"},"
                                   "{\"module_code\":\"%s\",\"channel_code\":\"meter_voltage\",\"metric_code\":\"voltage_v\",\"value\":%.1f,\"unit\":\"V\",\"quality\":\"good\"},"
                                   "{\"module_code\":\"%s\",\"channel_code\":\"meter_current\",\"metric_code\":\"current_a\",\"value\":%.2f,\"unit\":\"A\",\"quality\":\"good\"}"
                                   "]",
                                   mcode, mcode, v.energy_kwh, mcode, v.power_kw, mcode, v.voltage_v, mcode, v.current_a);
                    if (json_buf_append(&jb, tmp) != 0) {
                        return -4;
                    }
                } else if (strcmp(mcode, "soil_moisture_acquisition") == 0) {
                    module_soil_moisture_values_t v;
                    memset(&v, 0, sizeof(v));
                    (void)m->query_values(&v);
                    char tmp[256];
                    (void)snprintf(tmp, sizeof(tmp),
                                   "\",\"module_code\":\"%s\",\"channel_values\":[{"
                                   "\"module_code\":\"%s\",\"channel_code\":\"soil_moisture_1\",\"metric_code\":\"soil_moisture_vwc\","
                                   "\"value\":%.3f,\"unit\":null,\"quality\":\"good\""
                                   "}]",
                                   mcode, mcode, (double)v.soil_moisture_vwc);
                    if (json_buf_append(&jb, tmp) != 0) {
                        return -4;
                    }
                } else if (strcmp(mcode, "soil_temperature_acquisition") == 0) {
                    module_soil_temperature_values_t v;
                    memset(&v, 0, sizeof(v));
                    (void)m->query_values(&v);
                    char tmp[256];
                    (void)snprintf(tmp, sizeof(tmp),
                                   "\",\"module_code\":\"%s\",\"channel_values\":[{"
                                   "\"module_code\":\"%s\",\"channel_code\":\"soil_temperature_1\",\"metric_code\":\"soil_temperature_c\","
                                   "\"value\":%.2f,\"unit\":\"C\",\"quality\":\"good\""
                                   "}]",
                                   mcode, mcode, (double)v.soil_temperature_c);
                    if (json_buf_append(&jb, tmp) != 0) {
                        return -4;
                    }
                } else {
                    if (json_buf_append(&jb, "\",\"module_code\":\"") != 0) {
                        return -4;
                    }
                    if (json_escape_append(&jb, mcode) != 0) {
                        return -4;
                    }
                    if (json_buf_append(&jb, "\",\"note\":\"no values serializer\"") != 0) {
                        return -4;
                    }
                }
            }
        }
    } else {
        if (json_buf_append(&jb, "\",\"error\":\"unsupported_query\"") != 0) {
            return -4;
        }
    }

    if (proto_envelope_close_payload(&jb) != 0) {
        return -4;
    }
    return (int)jb.len;
}
