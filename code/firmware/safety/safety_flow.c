#include "safety_flow.h"

#include "common_identity.h"
#include "common_status.h"
#include "config_store.h"
#include "module_flow.h"
#include "module_meter.h"
#include "module_registry.h"
#include "net_connectivity.h"
#include "proto_codec_json.h"
#include "proto_envelope.h"
#include "proto_event_report.h"
#include "runtime_state.h"
#include "storage_runtime.h"
#include "workflow_local_access.h"
#include "workflow_voice.h"

#include "bsp_rtc.h"
#include "bsp_system.h"
#include "bsp_uart.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "proto_state_snapshot.h"

#define SAFETY_FLOW_ERR_INVALID_ACTION   (-1)
#define SAFETY_FLOW_ERR_BLOCKED          (-2)
#define SAFETY_FLOW_ERR_BUSY             (-3)
#define SAFETY_FLOW_ERR_SEQUENCE         (-4)
#define SAFETY_FLOW_ERR_METER            (-5)
#define SAFETY_FLOW_ERR_QUERY_SEND       (-6)
#define SAFETY_FLOW_ERR_AUTH_DENIED      (-7)
#define SAFETY_FLOW_ERR_NOT_RUNNING      (-8)
#define SAFETY_FLOW_ERR_UNSUPPORTED_LINK (-9)

#define SAFETY_DEFAULT_LEASE_SEC         300U
#define SAFETY_CARD_DEBOUNCE_MS          120U
#define SAFETY_CARD_RESULT_HOLD_MS       1000U
#define SAFETY_CARD_AUTH_TIMEOUT_MS      8000U
#define SAFETY_RECOVERY_RETRY_MS         3000U
#define SAFETY_PERSIST_MAGIC             0x504C4F53UL

typedef struct {
    card_flow_state_t state;
    uint32_t          state_since_ms;
    uint32_t          auth_timeout_ms;
    uint8_t           auth_granted;
    char              card_no[32];
    char              pending_session_ref[CTRL_SESSION_REF_LEN];
} card_flow_ctx_t;

typedef struct {
    uint32_t started_ms;
    uint32_t last_tick_ms;
    uint32_t offline_since_ms;
    uint32_t recovery_retry_due_ms;
    uint8_t  recovery_safe_off_pending;
    uint8_t  meter_energy_seen;
    float    last_seen_energy_kwh;
} safety_flow_ctx_t;

typedef struct {
    uint8_t pending;
    char    result_code[40];
    char    detail[64];
    char    card_no[32];
    char    session_ref[CTRL_SESSION_REF_LEN];
} card_auth_report_t;

typedef struct {
    uint8_t pending;
    char    event_code[8];
    char    result_code[40];
    char    detail[64];
    char    target_ref[24];
    char    session_ref[CTRL_SESSION_REF_LEN];
} pending_event_report_t;

static card_flow_ctx_t   s_card_ctx;
static safety_flow_ctx_t s_flow_ctx;
static card_auth_report_t s_card_report;
static pending_event_report_t s_pending_event_report;
static counter_reset_persist_t s_counter_reset_report;
static char s_last_authorized_card_no[32];

static void safety_logf(const char *tag, const char *fmt, ...);
static void safety_copy_string(char *dst, size_t cap, const char *src);
static void safety_copy_symbol(char *dst, size_t cap, const char *src);
static int safety_close_local_outputs(session_target_type_t target, linkage_mode_t linkage);
static void safety_schedule_recovery_safe_off(uint32_t delay_ms);
static void safety_try_recovery_safe_off(void);
static void safety_queue_event_report(const char *session_ref,
                                      const char *event_code,
                                      const char *result_code,
                                      const char *detail,
                                      const char *target_ref);
static void safety_try_emit_pending_event_report(void);
static void safety_queue_counter_reset_report(device_runtime_t *runtime,
                                              const char *reason_code,
                                              const char *session_ref,
                                              uint32_t runtime_sec,
                                              float total_m3,
                                              float energy_kwh);
static void safety_try_emit_counter_reset_report(void);
static void safety_bootstrap_meter_epoch(void);
static void safety_track_meter_epoch_events(void);
static void safety_clear_last_authorized_card(void);

static const safety_action_meta_t s_action_voice = {
    "audio_bus", 1500U, "voice_broadcast", 1U
};

static const safety_action_meta_t s_action_card = {
    "card_reader", 500U, "card_read", 0U
};

static const safety_action_meta_t s_action_cloud_auth = {
    "net_session", 8000U, "cloud_auth_request", 1U
};

static void safety_queue_card_auth_report(const char *result_code, const char *detail)
{
    memset(&s_card_report, 0, sizeof(s_card_report));
    s_card_report.pending = 1U;
    safety_copy_symbol(s_card_report.result_code, sizeof(s_card_report.result_code), result_code);
    safety_copy_symbol(s_card_report.detail, sizeof(s_card_report.detail), detail);
    safety_copy_symbol(s_card_report.card_no, sizeof(s_card_report.card_no), s_card_ctx.card_no);
    safety_copy_string(s_card_report.session_ref, sizeof(s_card_report.session_ref), s_card_ctx.pending_session_ref);
}

static void safety_clear_last_authorized_card(void)
{
    memset(s_last_authorized_card_no, 0, sizeof(s_last_authorized_card_no));
}

static void safety_try_emit_card_auth_report(void)
{
    const common_status_t *cs = common_status_get();

    if (s_card_report.pending == 0U || cs == NULL || !cs->online || !cs->tcp_connected) {
        return;
    }
    if (proto_event_report_send_min(s_card_report.session_ref[0] != '\0' ? s_card_report.session_ref : NULL,
                                    "car",
                                    s_card_report.result_code,
                                    s_card_report.detail,
                                    "card") > 0) {
        s_card_report.pending = 0U;
        runtime_state_inc_counter_event_report();
    }
}

static void safety_queue_event_report(const char *session_ref,
                                      const char *event_code,
                                      const char *result_code,
                                      const char *detail,
                                      const char *target_ref)
{
    memset(&s_pending_event_report, 0, sizeof(s_pending_event_report));
    s_pending_event_report.pending = 1U;
    safety_copy_string(s_pending_event_report.session_ref, sizeof(s_pending_event_report.session_ref), session_ref);
    safety_copy_symbol(s_pending_event_report.event_code, sizeof(s_pending_event_report.event_code), event_code);
    safety_copy_symbol(s_pending_event_report.result_code, sizeof(s_pending_event_report.result_code), result_code);
    safety_copy_symbol(s_pending_event_report.detail, sizeof(s_pending_event_report.detail), detail);
    safety_copy_symbol(s_pending_event_report.target_ref, sizeof(s_pending_event_report.target_ref), target_ref);
}

static void safety_try_emit_pending_event_report(void)
{
    const common_status_t *cs = common_status_get();

    if (s_pending_event_report.pending == 0U || cs == NULL || !cs->online || !cs->tcp_connected) {
        return;
    }
    if (proto_event_report_send_min(s_pending_event_report.session_ref[0] != '\0' ? s_pending_event_report.session_ref : NULL,
                                    s_pending_event_report.event_code,
                                    s_pending_event_report.result_code,
                                    s_pending_event_report.detail[0] != '\0' ? s_pending_event_report.detail : NULL,
                                    s_pending_event_report.target_ref[0] != '\0' ? s_pending_event_report.target_ref : NULL) > 0) {
        s_pending_event_report.pending = 0U;
        runtime_state_inc_counter_event_report();
    }
}

static uint32_t safety_next_meter_epoch(uint32_t current_epoch)
{
    return current_epoch == 0U ? 1U : (current_epoch + 1U);
}

static uint8_t safety_meter_identity_equals(uint8_t protocol_variant,
                                            const uint8_t addr_bcd[6],
                                            const device_runtime_t *runtime)
{
    if (runtime == NULL || runtime->meter_identity_valid == 0U) {
        return 0U;
    }
    if (runtime->meter_protocol_variant != protocol_variant) {
        return 0U;
    }
    return (uint8_t)(memcmp(runtime->meter_addr_bcd, addr_bcd, sizeof(runtime->meter_addr_bcd)) == 0 ? 1U : 0U);
}

static void safety_set_meter_identity(device_runtime_t *runtime,
                                      uint8_t protocol_variant,
                                      const uint8_t addr_bcd[6],
                                      uint8_t addr_valid)
{
    if (runtime == NULL) {
        return;
    }
    runtime->meter_identity_valid = addr_valid != 0U ? 1U : 0U;
    runtime->meter_protocol_variant = protocol_variant;
    memset(runtime->meter_addr_bcd, 0, sizeof(runtime->meter_addr_bcd));
    if (addr_bcd != NULL && addr_valid != 0U) {
        memcpy(runtime->meter_addr_bcd, addr_bcd, sizeof(runtime->meter_addr_bcd));
    }
}

static void safety_queue_counter_reset_report(device_runtime_t *runtime,
                                              const char *reason_code,
                                              const char *session_ref,
                                              uint32_t runtime_sec,
                                              float total_m3,
                                              float energy_kwh)
{
    if (runtime == NULL || runtime->meter_epoch == 0U) {
        return;
    }
    if ((s_counter_reset_report.pending != 0U && s_counter_reset_report.epoch == runtime->meter_epoch) ||
        (runtime->counter_reset.pending != 0U && runtime->counter_reset.epoch == runtime->meter_epoch)) {
        return;
    }
    if (runtime->last_reported_counter_reset_epoch == runtime->meter_epoch) {
        return;
    }

    memset(&s_counter_reset_report, 0, sizeof(s_counter_reset_report));
    s_counter_reset_report.pending = 1U;
    s_counter_reset_report.epoch = runtime->meter_epoch;
    safety_copy_symbol(s_counter_reset_report.reason, sizeof(s_counter_reset_report.reason), reason_code);
    safety_copy_string(s_counter_reset_report.session_ref, sizeof(s_counter_reset_report.session_ref), session_ref);
    s_counter_reset_report.runtime_sec = runtime_sec;
    s_counter_reset_report.total_m3 = total_m3;
    s_counter_reset_report.energy_kwh = energy_kwh;

    runtime->counter_reset = s_counter_reset_report;
    (void)storage_runtime_save(runtime);
    runtime_state_set_meter_epoch(runtime->meter_epoch);
    safety_logf("[METER] ", "counter_reset queued epoch=%lu reason=%s session=%s",
                (unsigned long)runtime->meter_epoch,
                s_counter_reset_report.reason,
                s_counter_reset_report.session_ref);
}

static void safety_try_emit_counter_reset_report(void)
{
    const common_status_t *cs = common_status_get();
    device_runtime_t runtime;
    char reason[8];

    if (s_counter_reset_report.pending == 0U || cs == NULL || !cs->online || !cs->tcp_connected) {
        return;
    }
    if (proto_event_report_send_counter_reset(
            s_counter_reset_report.session_ref[0] != '\0' ? s_counter_reset_report.session_ref : NULL,
            s_counter_reset_report.reason,
            s_counter_reset_report.epoch,
            s_counter_reset_report.runtime_sec,
            s_counter_reset_report.total_m3,
            s_counter_reset_report.energy_kwh) <= 0) {
        return;
    }

    memset(&runtime, 0, sizeof(runtime));
    (void)storage_runtime_load(&runtime);
    safety_copy_symbol(reason, sizeof(reason), s_counter_reset_report.reason);
    runtime.last_reported_counter_reset_epoch = s_counter_reset_report.epoch;
    memset(&runtime.counter_reset, 0, sizeof(runtime.counter_reset));
    (void)storage_runtime_save(&runtime);

    s_counter_reset_report.pending = 0U;
    runtime_state_inc_counter_event_report();
    safety_logf("[METER] ", "counter_reset reported epoch=%lu reason=%s",
                (unsigned long)runtime.last_reported_counter_reset_epoch,
                reason);
}

static void safety_bootstrap_meter_epoch(void)
{
    device_runtime_t runtime;
    const char *reason = "rbt";
    const char *session_ref = NULL;
    uint32_t runtime_sec = 0U;
    float total_m3 = 0.0f;
    float energy_kwh = 0.0f;

    memset(&runtime, 0, sizeof(runtime));
    if (storage_runtime_load(&runtime) != 0) {
        runtime.meter_epoch = 1U;
        runtime.last_reported_counter_reset_epoch = 0U;
        (void)storage_runtime_save(&runtime);
        runtime_state_set_meter_epoch(runtime.meter_epoch);
        return;
    }

    if (runtime.meter_epoch == 0U) {
        runtime.meter_epoch = 1U;
        (void)storage_runtime_save(&runtime);
        runtime_state_set_meter_epoch(runtime.meter_epoch);
        if (runtime.counter_reset.pending != 0U) {
            s_counter_reset_report = runtime.counter_reset;
        }
        return;
    }

    if (runtime.power_loss_persist.last_session_active != 0U) {
        reason = "abr";
        session_ref = runtime.power_loss_persist.session_ref;
        runtime_sec = runtime.power_loss_persist.runtime_sec;
        total_m3 = runtime.power_loss_persist.last_total_m3;
        energy_kwh = runtime.power_loss_persist.last_energy_kwh;
    }

    runtime.meter_epoch = safety_next_meter_epoch(runtime.meter_epoch);
    safety_queue_counter_reset_report(&runtime, reason, session_ref, runtime_sec, total_m3, energy_kwh);
}

static void safety_log_tagged(const char *tag, const char *msg)
{
    bsp_debug_log(tag);
    bsp_debug_log(msg);
    bsp_debug_log("\r\n");
}

static void safety_logf(const char *tag, const char *fmt, ...)
{
    char    line[192];
    va_list ap;

    va_start(ap, fmt);
    (void)vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    safety_log_tagged(tag, line);
}

static void safety_copy_string(char *dst, size_t cap, const char *src)
{
    size_t i = 0U;

    if (dst == NULL || cap == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1U < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void safety_copy_symbol(char *dst, size_t cap, const char *src)
{
    size_t i = 0U;

    if (dst == NULL || cap == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1U < cap) {
        char ch = src[i];
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' ||
            ch == '-') {
            dst[i] = ch;
        } else {
            dst[i] = '_';
        }
        i++;
    }
    dst[i] = '\0';
}

static void safety_format_fixed(char *dst, size_t cap, float value, uint8_t frac_digits)
{
    uint32_t scale = 1U;
    int32_t scaled;
    uint32_t abs_scaled;
    uint32_t int_part;
    uint32_t frac_part;

    if (dst == NULL || cap == 0U) {
        return;
    }
    if (frac_digits > 3U) {
        frac_digits = 3U;
    }
    for (uint8_t i = 0U; i < frac_digits; i++) {
        scale *= 10U;
    }
    if (value >= 0.0f) {
        scaled = (int32_t)(value * (float)scale + 0.5f);
    } else {
        scaled = (int32_t)(value * (float)scale - 0.5f);
    }
    abs_scaled = (scaled < 0) ? (uint32_t)(-scaled) : (uint32_t)scaled;
    int_part = abs_scaled / scale;
    frac_part = abs_scaled % scale;
    if (frac_digits == 0U) {
        (void)snprintf(dst, cap, "%s%lu", scaled < 0 ? "-" : "", (unsigned long)int_part);
    } else {
        (void)snprintf(dst, cap, "%s%lu.%0*lu",
                       scaled < 0 ? "-" : "",
                       (unsigned long)int_part,
                       (int)frac_digits,
                       (unsigned long)frac_part);
    }
}

static const char *safety_meter_source_name(void)
{
    const char *meter_source = module_meter_source_name();

    if (meter_source != NULL && meter_source[0] != '\0' &&
        strcmp(meter_source, "unknown") != 0) {
        return meter_source;
    }
    return "unknown";
}

static runtime_workflow_state_t map_main_state(session_main_state_t state)
{
    switch (state) {
    case SESSION_STATE_BOOTING: return RUNTIME_WORKFLOW_BOOTING;
    case SESSION_STATE_NOT_READY: return RUNTIME_WORKFLOW_NOT_READY;
    case SESSION_STATE_READY_IDLE: return RUNTIME_WORKFLOW_READY_IDLE;
    case SESSION_STATE_AUTH_PENDING: return RUNTIME_WORKFLOW_AUTH_PENDING;
    case SESSION_STATE_PRE_START_METERING: return RUNTIME_WORKFLOW_PRE_START_METERING;
    case SESSION_STATE_START_SEQUENCE: return RUNTIME_WORKFLOW_START_SEQUENCE;
    case SESSION_STATE_RUNNING: return RUNTIME_WORKFLOW_RUNNING;
    case SESSION_STATE_STOP_SEQUENCE: return RUNTIME_WORKFLOW_STOP_SEQUENCE;
    case SESSION_STATE_POST_STOP_METERING: return RUNTIME_WORKFLOW_POST_STOP_METERING;
    case SESSION_STATE_BLOCKED: return RUNTIME_WORKFLOW_BLOCKED;
    case SESSION_STATE_FAULT_LATCHED: return RUNTIME_WORKFLOW_FAULT_LATCHED;
    case SESSION_STATE_RECOVERY_LOCKED: return RUNTIME_WORKFLOW_RECOVERY_LOCKED;
    default: return RUNTIME_WORKFLOW_NOT_READY;
    }
}

static session_main_state_t map_runtime_main_state(runtime_workflow_state_t state)
{
    switch (state) {
    case RUNTIME_WORKFLOW_BOOTING: return SESSION_STATE_BOOTING;
    case RUNTIME_WORKFLOW_NOT_READY: return SESSION_STATE_NOT_READY;
    case RUNTIME_WORKFLOW_READY_IDLE: return SESSION_STATE_READY_IDLE;
    case RUNTIME_WORKFLOW_AUTH_PENDING: return SESSION_STATE_AUTH_PENDING;
    case RUNTIME_WORKFLOW_PRE_START_METERING: return SESSION_STATE_PRE_START_METERING;
    case RUNTIME_WORKFLOW_START_SEQUENCE: return SESSION_STATE_START_SEQUENCE;
    case RUNTIME_WORKFLOW_RUNNING: return SESSION_STATE_RUNNING;
    case RUNTIME_WORKFLOW_STOP_SEQUENCE: return SESSION_STATE_STOP_SEQUENCE;
    case RUNTIME_WORKFLOW_POST_STOP_METERING: return SESSION_STATE_POST_STOP_METERING;
    case RUNTIME_WORKFLOW_BLOCKED: return SESSION_STATE_BLOCKED;
    case RUNTIME_WORKFLOW_FAULT_LATCHED: return SESSION_STATE_FAULT_LATCHED;
    case RUNTIME_WORKFLOW_RECOVERY_LOCKED: return SESSION_STATE_RECOVERY_LOCKED;
    default: return SESSION_STATE_NOT_READY;
    }
}

static const char *voice_prompt_text(voice_prompt_code_t code)
{
    switch (code) {
    case VOICE_DEVICE_BOOTING: return "device_booting";
    case VOICE_DEVICE_NOT_READY: return "device_not_ready";
    case VOICE_DEVICE_CONFIG_MISSING: return "device_config_missing";
    case VOICE_NETWORK_UNAVAILABLE: return "network_unavailable";
    case VOICE_AUTH_CHECKING: return "auth_checking";
    case VOICE_AUTH_GRANTED: return "auth_granted";
    case VOICE_AUTH_DENIED: return "auth_denied";
    case VOICE_METER_QUERY_FAILED: return "meter_query_failed";
    case VOICE_STARTING_PUMP: return "starting_pump";
    case VOICE_STARTING_VALVE: return "starting_valve";
    case VOICE_START_SUCCESS: return "start_success";
    case VOICE_START_FAILED: return "start_failed";
    case VOICE_ALREADY_RUNNING: return "already_running";
    case VOICE_STOPPING: return "stopping";
    case VOICE_STOPPED: return "stopped";
    case VOICE_PROTECTION_TRIGGERED: return "protection_triggered";
    case VOICE_POWER_INTERRUPTED: return "power_interrupted";
    case VOICE_POWER_RESTORED_SELF_CHECK: return "power_restored_self_check";
    case VOICE_RECOVERY_LOCKED_CONFIRM_REQUIRED: return "recovery_locked_confirm_required";
    default: return "device_not_ready";
    }
}

static void safety_voice_prompt(voice_prompt_code_t code)
{
    runtime_state_set_voice_prompt(code);
    workflow_voice_prompt_once(voice_prompt_text(code), "safety_flow", 500U);
    safety_logf("[VOICE] ", "prompt=%s", runtime_state_voice_prompt_name(code));
    (void)s_action_voice;
}

static void safety_set_main_state(session_main_state_t state)
{
    runtime_state_set_workflow_state(map_main_state(state));
}

static void safety_set_card_state(card_flow_state_t state)
{
    runtime_state_set_card_state(state);
    s_card_ctx.state = state;
    s_card_ctx.state_since_ms = s_flow_ctx.last_tick_ms;
}

static void safety_set_safe_state(safety_state_t state)
{
    runtime_state_set_safe_state(state);
}

static bool safety_session_active(const runtime_state_t *rs)
{
    return rs != NULL &&
           (rs->run_state == RUNTIME_RUN_STARTING ||
            rs->run_state == RUNTIME_RUN_RUNNING ||
            rs->run_state == RUNTIME_RUN_STOPPING);
}

static bool safety_target_has_pump(session_target_type_t target)
{
    return target == TARGET_PUMP || target == TARGET_PUMP_WITH_VALVE;
}

static bool safety_target_has_valve(session_target_type_t target)
{
    return target == TARGET_VALVE || target == TARGET_PUMP_WITH_VALVE;
}

static uint32_t safety_now_sec(void)
{
    return s_flow_ctx.last_tick_ms / 1000U;
}

static void safety_generate_session_ref(char *out, size_t out_cap, const char *prefix)
{
    const controller_identity_t *id = common_identity_get();
    const char *imei = (id != NULL && id->imei[0] != '\0') ? id->imei : "device";

    if (out == NULL || out_cap == 0U) {
        return;
    }
    (void)snprintf(out, out_cap, "%s_%s_%lu",
                   prefix != NULL ? prefix : "session",
                   imei,
                   (unsigned long)proto_envelope_take_seq_no(0U));
}

static session_target_type_t safety_default_target(void)
{
    const device_config_t *cfg = config_store_active();

    if (cfg == NULL) {
        return TARGET_PUMP;
    }
    if ((cfg->feature_modules.breaker_control != 0U ||
         cfg->feature_modules.pump_direct_control != 0U ||
         cfg->feature_modules.pump_vfd_control != 0U) &&
        cfg->feature_modules.single_valve_control != 0U &&
        cfg->control_config.linkage_mode == LINKAGE_LOCAL_INTEGRATED) {
        return TARGET_PUMP_WITH_VALVE;
    }
    if (cfg->feature_modules.breaker_control != 0U ||
        cfg->feature_modules.pump_direct_control != 0U ||
        cfg->feature_modules.pump_vfd_control != 0U) {
        return TARGET_PUMP;
    }
    return TARGET_VALVE;
}

static linkage_mode_t safety_default_linkage(void)
{
    const device_config_t *cfg = config_store_active();

    return cfg != NULL ? cfg->control_config.linkage_mode : LINKAGE_LOCAL_INTEGRATED;
}

static settlement_mode_t safety_default_settlement(void)
{
    return SETTLEMENT_BY_ENERGY_KWH;
}

static int safety_parse_target_type(const char *json, session_target_type_t *out)
{
    char buf[24];

    if (json == NULL || out == NULL) {
        return -1;
    }
    if (proto_json_get_string(json, "target_type", buf, sizeof(buf)) != 0) {
        *out = safety_default_target();
        return 0;
    }
    if (strcmp(buf, "pump") == 0) {
        *out = TARGET_PUMP;
    } else if (strcmp(buf, "valve") == 0) {
        *out = TARGET_VALVE;
    } else if (strcmp(buf, "pump_with_valve") == 0) {
        *out = TARGET_PUMP_WITH_VALVE;
    } else {
        return -1;
    }
    return 0;
}

static int safety_parse_linkage_mode(const char *json, linkage_mode_t *out)
{
    char buf[32];

    if (json == NULL || out == NULL) {
        return -1;
    }
    if (proto_json_get_string(json, "linkage_mode", buf, sizeof(buf)) != 0) {
        *out = safety_default_linkage();
        return 0;
    }
    if (strcmp(buf, "local_integrated") == 0) {
        *out = LINKAGE_LOCAL_INTEGRATED;
    } else if (strcmp(buf, "platform_orchestrated") == 0) {
        *out = LINKAGE_PLATFORM_ORCHESTRATED;
    } else {
        return -1;
    }
    return 0;
}

static int safety_parse_settlement_mode(const char *json, settlement_mode_t *out)
{
    char buf[32];

    if (json == NULL || out == NULL) {
        return -1;
    }
    if (proto_json_get_string(json, "settlement_mode", buf, sizeof(buf)) != 0) {
        *out = safety_default_settlement();
        return 0;
    }
    if (strcmp(buf, "by_duration_hour") == 0) {
        *out = SETTLEMENT_BY_DURATION_HOUR;
    } else if (strcmp(buf, "by_energy_kwh") == 0) {
        *out = SETTLEMENT_BY_ENERGY_KWH;
    } else if (strcmp(buf, "by_water_m3") == 0) {
        *out = SETTLEMENT_BY_WATER_M3;
    } else {
        return -1;
    }
    return 0;
}

static int safety_meter_read_snapshot(meter_snapshot_t *out)
{
    module_meter_values_t values;
    char voltage[16];
    char current[16];
    char power[16];
    char energy[16];

    if (out == NULL) {
        return -1;
    }
    memset(&values, 0, sizeof(values));
    if (module_meter_query_values(&values) != 0 || values.quality == 0U) {
        memset(out, 0, sizeof(*out));
        safety_log_tagged("[METER] ", "meter_read_snapshot failed");
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->voltage_v = (float)values.voltage_v;
    out->current_a = (float)values.current_a;
    out->power_kw = (float)values.power_kw;
    out->energy_kwh = (float)values.energy_kwh;
    out->phase_status = (out->voltage_v > 0.0f) ? PHASE_STATUS_OK : PHASE_STATUS_LOSS;
    out->power_factor = (out->voltage_v > 0.0f && out->current_a > 0.0f)
        ? (out->power_kw * 1000.0f) / (out->voltage_v * out->current_a * 1.732f)
        : 0.0f;
    if (out->power_factor < 0.0f) {
        out->power_factor = 0.0f;
    }
    if (out->power_factor > 1.0f) {
        out->power_factor = 1.0f;
    }
    out->frequency_hz = 50.0f;
    out->valid = 1U;
    safety_format_fixed(voltage, sizeof(voltage), out->voltage_v, 1U);
    safety_format_fixed(current, sizeof(current), out->current_a, 1U);
    safety_format_fixed(power, sizeof(power), out->power_kw, 2U);
    safety_format_fixed(energy, sizeof(energy), out->energy_kwh, 2U);
    safety_logf("[METER] ", "source=%s snapshot v=%s a=%s kw=%s kwh=%s",
                safety_meter_source_name(), voltage, current, power, energy);
    return 0;
}

static int safety_flow_read_snapshot(flow_snapshot_t *out)
{
    module_flow_values_t values;
    char flow[16];
    char total[16];

    if (out == NULL) {
        return -1;
    }
    memset(&values, 0, sizeof(values));
    if (module_flow_query_values(&values) != 0 || values.quality == 0U) {
        memset(out, 0, sizeof(*out));
        safety_log_tagged("[METER] ", "flow_read_snapshot failed");
        return -1;
    }
    out->flow_m3h = values.instant_m3h;
    out->total_m3 = (float)values.total_m3;
    out->valid = 1U;
    safety_format_fixed(flow, sizeof(flow), out->flow_m3h, 2U);
    safety_format_fixed(total, sizeof(total), out->total_m3, 2U);
    safety_logf("[METER] ", "source=%s flow q=%s total=%s",
                module_flow_source_name(), flow, total);
    return 0;
}

static void safety_set_lease(const char *session_ref, uint16_t lease_sec, uint8_t keepalive_required)
{
    session_lease_t lease;

    memset(&lease, 0, sizeof(lease));
    if (session_ref != NULL) {
        safety_copy_string(lease.session_ref, sizeof(lease.session_ref), session_ref);
    }
    lease.session_lease_sec = lease_sec;
    lease.keepalive_required = keepalive_required;
    lease.last_lease_refresh_at = safety_now_sec();
    lease.lease_expire_at = lease.last_lease_refresh_at + lease_sec;
    runtime_state_set_session_lease(&lease);
}

static void safety_clear_lease(void)
{
    runtime_state_set_session_lease(NULL);
}

static void safety_update_summary(bool finalized)
{
    runtime_state_t *rs = runtime_state_mutable();
    const meter_snapshot_t *meter_stop = rs->meter_stop.valid != 0U ? &rs->meter_stop : &rs->meter_last;
    const flow_snapshot_t  *flow_stop = rs->flow_stop.valid != 0U ? &rs->flow_stop : &rs->flow_last;
    char start_kwh[16];
    char stop_kwh[16];
    char delta_kwh[16];
    char start_m3[16];
    char stop_m3[16];
    char delta_m3[16];

    safety_copy_string(rs->summary.meter_source, sizeof(rs->summary.meter_source), safety_meter_source_name());
    rs->summary.runtime_sec = rs->runtime_sec;
    rs->summary.start_energy_kwh = rs->meter_start.valid != 0U ? rs->meter_start.energy_kwh : 0.0f;
    rs->summary.stop_energy_kwh = meter_stop->valid != 0U ? meter_stop->energy_kwh : rs->summary.start_energy_kwh;
    rs->summary.delta_energy_kwh = rs->summary.stop_energy_kwh - rs->summary.start_energy_kwh;
    rs->summary.start_total_m3 = rs->flow_start.valid != 0U ? rs->flow_start.total_m3 : 0.0f;
    rs->summary.stop_total_m3 = flow_stop->valid != 0U ? flow_stop->total_m3 : rs->summary.start_total_m3;
    rs->summary.delta_total_m3 = rs->summary.stop_total_m3 - rs->summary.start_total_m3;
    rs->summary.last_voltage_v = rs->voltage_v;
    rs->summary.last_current_a = rs->current_a;
    rs->summary.last_power_kw = rs->power_kw;
    rs->summary.stop_reason = rs->stop_reason;
    rs->summary.finalized = finalized ? 1U : 0U;
    safety_format_fixed(start_kwh, sizeof(start_kwh), rs->summary.start_energy_kwh, 3U);
    safety_format_fixed(stop_kwh, sizeof(stop_kwh), rs->summary.stop_energy_kwh, 3U);
    safety_format_fixed(delta_kwh, sizeof(delta_kwh), rs->summary.delta_energy_kwh, 3U);
    safety_format_fixed(start_m3, sizeof(start_m3), rs->summary.start_total_m3, 3U);
    safety_format_fixed(stop_m3, sizeof(stop_m3), rs->summary.stop_total_m3, 3U);
    safety_format_fixed(delta_m3, sizeof(delta_m3), rs->summary.delta_total_m3, 3U);
    safety_logf("[METER] ",
                "summary source=%s runtime=%lu start_kwh=%s stop_kwh=%s delta_kwh=%s start_m3=%s stop_m3=%s delta_m3=%s finalized=%u",
                rs->summary.meter_source,
                (unsigned long)rs->summary.runtime_sec,
                start_kwh,
                stop_kwh,
                delta_kwh,
                start_m3,
                stop_m3,
                delta_m3,
                (unsigned)rs->summary.finalized);
}

static void safety_persist_runtime(bool session_active, bool finalized)
{
    device_runtime_t runtime;
    runtime_state_t *rs = runtime_state_mutable();
    const device_config_t *cfg = config_store_active();

    memset(&runtime, 0, sizeof(runtime));
    (void)storage_runtime_load(&runtime);

    runtime.power_loss_persist.magic = SAFETY_PERSIST_MAGIC;
    runtime.power_loss_persist.last_session_active = session_active ? 1U : 0U;
    runtime.power_loss_persist.last_main_state = map_runtime_main_state(rs->workflow_state);
    runtime.power_loss_persist.last_target_type = rs->target_type;
    runtime.power_loss_persist.last_linkage_mode = rs->linkage_mode;
    runtime.power_loss_persist.valve_fail_safe_mode =
        cfg != NULL ? cfg->control_config.valve_fail_safe_mode : VALVE_FAIL_CLOSE;
    runtime.power_loss_persist.pump_output_fail_safe =
        cfg != NULL ? cfg->control_config.pump_output_fail_safe : PUMP_FAIL_SAFE_DEENERGIZE;
    runtime.power_loss_persist.last_stop_reason = rs->stop_reason;
    runtime.power_loss_persist.power_loss_high_risk = rs->power_domains.power_loss_high_risk;
    runtime.power_loss_persist.finalized = finalized ? 1U : 0U;
    runtime.power_loss_persist.runtime_sec = rs->runtime_sec;
    runtime.power_loss_persist.last_energy_kwh = rs->energy_kwh;
    runtime.power_loss_persist.last_total_m3 = rs->total_m3;
    safety_copy_string(runtime.power_loss_persist.session_ref, sizeof(runtime.power_loss_persist.session_ref),
                       rs->session_ref);

    memset(&runtime.active_session, 0, sizeof(runtime.active_session));
    if (session_active) {
        safety_copy_string(runtime.active_session.session_id, sizeof(runtime.active_session.session_id),
                           rs->session_ref);
        runtime.active_session.started_at_utc = safety_now_sec();
    }

    runtime.meter_epoch = rs->meter_epoch;
    if (runtime.meter_epoch == 0U) {
        runtime.meter_epoch = 1U;
    }

    (void)storage_runtime_save(&runtime);
}

static void safety_track_meter_epoch_events(void)
{
    runtime_state_t *rs = runtime_state_mutable();
    device_runtime_t runtime;
    uint8_t protocol_variant = 0U;
    uint8_t addr_valid = 0U;
    uint8_t addr_bcd[6] = {0U};

    memset(&runtime, 0, sizeof(runtime));
    if (storage_runtime_load(&runtime) != 0) {
        runtime.meter_epoch = rs->meter_epoch != 0U ? rs->meter_epoch : 1U;
        runtime.last_reported_counter_reset_epoch = 0U;
    }
    if (runtime.meter_epoch == 0U) {
        runtime.meter_epoch = rs->meter_epoch != 0U ? rs->meter_epoch : 1U;
    }
    if (rs->meter_epoch != runtime.meter_epoch) {
        runtime_state_set_meter_epoch(runtime.meter_epoch);
    }

    if (module_meter_get_identity(&protocol_variant, addr_bcd, &addr_valid) != 0U) {
        addr_valid = 0U;
    }
    if (addr_valid != 0U) {
        if (runtime.meter_identity_valid == 0U) {
            safety_set_meter_identity(&runtime, protocol_variant, addr_bcd, 1U);
            (void)storage_runtime_save(&runtime);
        } else if (safety_meter_identity_equals(protocol_variant, addr_bcd, &runtime) == 0U) {
            runtime.meter_epoch = safety_next_meter_epoch(runtime.meter_epoch);
            safety_set_meter_identity(&runtime, protocol_variant, addr_bcd, 1U);
            safety_queue_counter_reset_report(&runtime,
                                              "mtr",
                                              rs->session_ref,
                                              rs->runtime_sec,
                                              rs->total_m3,
                                              rs->energy_kwh);
            s_flow_ctx.meter_energy_seen = (rs->meter_last.valid != 0U) ? 1U : 0U;
            s_flow_ctx.last_seen_energy_kwh = rs->energy_kwh;
            return;
        }
    }

    if (rs->meter_last.valid == 0U) {
        return;
    }
    if (s_flow_ctx.meter_energy_seen == 0U) {
        s_flow_ctx.meter_energy_seen = 1U;
        s_flow_ctx.last_seen_energy_kwh = rs->energy_kwh;
        return;
    }
    if (rs->energy_kwh + 0.01f < s_flow_ctx.last_seen_energy_kwh) {
        runtime.meter_epoch = safety_next_meter_epoch(runtime.meter_epoch);
        safety_queue_counter_reset_report(&runtime,
                                          "clr",
                                          rs->session_ref,
                                          rs->runtime_sec,
                                          rs->total_m3,
                                          rs->energy_kwh);
        s_flow_ctx.last_seen_energy_kwh = rs->energy_kwh;
        return;
    }
    if (rs->energy_kwh > s_flow_ctx.last_seen_energy_kwh) {
        s_flow_ctx.last_seen_energy_kwh = rs->energy_kwh;
    }
}

static void safety_schedule_recovery_safe_off(uint32_t delay_ms)
{
    s_flow_ctx.recovery_safe_off_pending = 1U;
    s_flow_ctx.recovery_retry_due_ms = s_flow_ctx.last_tick_ms + delay_ms;
}

static void safety_try_recovery_safe_off(void)
{
    runtime_state_t *rs = runtime_state_mutable();

    if (s_flow_ctx.recovery_safe_off_pending == 0U ||
        rs->workflow_state != RUNTIME_WORKFLOW_RECOVERY_LOCKED ||
        !safety_target_has_pump(rs->target_type) ||
        (int32_t)(s_flow_ctx.last_tick_ms - s_flow_ctx.recovery_retry_due_ms) < 0) {
        return;
    }

    if (safety_close_local_outputs(rs->target_type, rs->linkage_mode) == 0) {
        s_flow_ctx.recovery_safe_off_pending = 0U;
        runtime_state_set_pump_state(RUNTIME_PUMP_STOPPED);
        if (safety_target_has_valve(rs->target_type)) {
            runtime_state_set_valve_state(RUNTIME_VALVE_CLOSED);
        }
        safety_logf("[SAFE] ", "recovery safe-off confirmed session=%s stop_reason=%s",
                    rs->session_ref,
                    runtime_state_stop_reason_name(rs->stop_reason));
        safety_persist_runtime(true, false);
        return;
    }

    safety_logf("[SAFE] ", "recovery safe-off retry pending session=%s stop_reason=%s next_in_ms=%lu",
                rs->session_ref,
                runtime_state_stop_reason_name(rs->stop_reason),
                (unsigned long)SAFETY_RECOVERY_RETRY_MS);
    s_flow_ctx.recovery_retry_due_ms = s_flow_ctx.last_tick_ms + SAFETY_RECOVERY_RETRY_MS;
}

static bool safety_meter_required(settlement_mode_t mode)
{
    return mode == SETTLEMENT_BY_ENERGY_KWH;
}

static bool safety_flow_required(settlement_mode_t mode)
{
    return mode == SETTLEMENT_BY_WATER_M3;
}

static int safety_ready_check(uint8_t require_network, settlement_mode_t settlement,
                              uint8_t allow_recovery_override, blocked_reason_code_t *reason_out)
{
    const runtime_state_t *rs = runtime_state_get();
    const common_status_t *cs = common_status_get();
    const device_config_t *cfg = config_store_active();
    meter_snapshot_t meter;
    flow_snapshot_t flow;

    if (reason_out != NULL) {
        *reason_out = BLOCKED_NONE;
    }
    if (cfg == NULL) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_NOT_READY_CONFIG;
        }
        return -1;
    }
    if (rs->boot_ok == 0U) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_NOT_READY_BOOTING;
        }
        return -1;
    }
    if (rs->protection_active || rs->protection_state == SAFE_STATE_LATCHED) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_PROTECTION_LATCHED;
        }
        return -1;
    }
    if (!allow_recovery_override && rs->workflow_state == RUNTIME_WORKFLOW_RECOVERY_LOCKED) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_RECOVERY_LOCKED;
        }
        return -1;
    }
    if (safety_session_active(rs)) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_ALREADY_RUNNING;
        }
        return -1;
    }
    if (rs->voltage_v > 0.0f) {
        if (cfg->protection_config.over_voltage_protection != 0U &&
            cfg->protection_config.over_voltage_limit_v > 0.0f &&
            rs->voltage_v > cfg->protection_config.over_voltage_limit_v) {
            if (reason_out != NULL) {
                *reason_out = BLOCKED_OVER_VOLTAGE;
            }
            return -1;
        }
        if (cfg->protection_config.under_voltage_protection != 0U &&
            cfg->protection_config.under_voltage_limit_v > 0.0f &&
            rs->voltage_v < cfg->protection_config.under_voltage_limit_v) {
            if (reason_out != NULL) {
                *reason_out = BLOCKED_UNDER_VOLTAGE;
            }
            return -1;
        }
    }
    if (rs->meter_last.valid != 0U && rs->meter_last.phase_status == PHASE_STATUS_LOSS) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_PHASE_LOSS;
        }
        return -1;
    }
    if (cfg->protection_config.pressure_low_limit > 0.0f &&
        rs->pressure_mpa > 0.0f &&
        rs->pressure_mpa < cfg->protection_config.pressure_low_limit) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_PRESSURE_ABNORMAL;
        }
        return -1;
    }
    if (require_network != 0U && (cs == NULL || !cs->online || !cs->tcp_connected || !cs->registered_once)) {
        if (reason_out != NULL) {
            *reason_out = BLOCKED_NETWORK_UNAVAILABLE;
        }
        return -1;
    }
    if (safety_meter_required(settlement)) {
        if (safety_meter_read_snapshot(&meter) != 0) {
            if (reason_out != NULL) {
                *reason_out = BLOCKED_METER_UNAVAILABLE;
            }
            return -1;
        }
    }
    if (safety_flow_required(settlement)) {
        if (safety_flow_read_snapshot(&flow) != 0) {
            if (reason_out != NULL) {
                *reason_out = BLOCKED_FLOW_SENSOR_UNAVAILABLE;
            }
            return -1;
        }
    }
    return 0;
}

static void safety_apply_blocked(blocked_reason_code_t reason)
{
    const runtime_state_t *rs = runtime_state_get();

    runtime_state_set_blocked_reason(reason);
    runtime_state_set_last_event(EVT_READY_CHECK_FAIL);
    if (reason == BLOCKED_ALREADY_RUNNING && safety_session_active(rs)) {
        safety_voice_prompt(VOICE_ALREADY_RUNNING);
        safety_logf("[SAFE] ", "blocked reason=%s", runtime_state_blocked_reason_name(reason));
        return;
    }
    safety_set_main_state(SESSION_STATE_BLOCKED);
    runtime_state_set_run_state(RUNTIME_RUN_STANDBY);
    safety_set_safe_state(SAFE_STATE_REJECT_START);
    if (reason == BLOCKED_NOT_READY_CONFIG) {
        safety_voice_prompt(VOICE_DEVICE_CONFIG_MISSING);
    } else if (reason == BLOCKED_ALREADY_RUNNING) {
        safety_voice_prompt(VOICE_ALREADY_RUNNING);
    } else if (reason == BLOCKED_NETWORK_UNAVAILABLE) {
        safety_voice_prompt(VOICE_NETWORK_UNAVAILABLE);
    } else {
        safety_voice_prompt(VOICE_DEVICE_NOT_READY);
    }
    safety_logf("[SAFE] ", "blocked reason=%s", runtime_state_blocked_reason_name(reason));
}

static uint8_t safety_run_local_action(const char *module_code, const char *action_code)
{
    const module_ops_t *ops = module_registry_get(module_code);

    if (ops == NULL || ops->execute_action == NULL) {
        return 1U;
    }
    return ops->execute_action(action_code, NULL, NULL);
}

static uint8_t safety_run_pump_action(uint8_t start)
{
    const control_config_t *control = config_store_control();
    pump_control_mode_t mode = PUMP_CONTROL_RELAY_DIRECT;

    if (control != NULL) {
        mode = control->pump_control_mode;
    }

    switch (mode) {
    case PUMP_CONTROL_METER_BREAKER_485:
        return safety_run_local_action("electric_meter_modbus",
                                       start != 0U ? "close_breaker" : "open_breaker");
    case PUMP_CONTROL_CONTACTOR_DIRECT:
    case PUMP_CONTROL_RELAY_DIRECT:
    default:
        return safety_run_local_action("pump_vfd_control",
                                       start != 0U ? "start_vfd" : "stop_vfd");
    }
}

static void safety_maybe_delay_ms(uint32_t delay_ms, const char *stage)
{
    if (delay_ms == 0U) {
        return;
    }
    safety_logf("[FLOW] ", "delay stage=%s delay_ms=%lu",
                stage != NULL ? stage : "",
                (unsigned long)delay_ms);
    bsp_system_delay_ms(delay_ms);
}

static void safety_refresh_runtime_snapshot(void)
{
    runtime_state_t *rs = runtime_state_mutable();
    meter_snapshot_t meter;
    flow_snapshot_t flow;

    memset(&meter, 0, sizeof(meter));
    memset(&flow, 0, sizeof(flow));

    if (safety_meter_read_snapshot(&meter) == 0) {
        runtime_state_set_meter_snapshots(rs->meter_start.valid != 0U ? &rs->meter_start : NULL, &meter, NULL);
        rs->voltage_v = meter.voltage_v;
        rs->current_a = meter.current_a;
        rs->power_kw = meter.power_kw;
        rs->energy_kwh = meter.energy_kwh;
    }
    if (safety_flow_read_snapshot(&flow) == 0) {
        runtime_state_set_flow_snapshots(rs->flow_start.valid != 0U ? &rs->flow_start : NULL, &flow, NULL);
        rs->flow_m3h = flow.flow_m3h;
        rs->total_m3 = flow.total_m3;
    }
}

static void safety_send_state_snapshot_now(const char *reason)
{
    char message[512];
    int len;
    int rc;

    safety_refresh_runtime_snapshot();
    len = proto_state_snapshot_build(message, sizeof(message));
    if (len <= 0) {
        safety_logf("[FLOW] ", "state snapshot build failed reason=%s",
                    reason != NULL ? reason : "");
        return;
    }
    rc = net_connectivity_send_json(message, (size_t)len);
    if (rc >= 0) {
        runtime_state_inc_counter_snapshot_ok();
        safety_logf("[FLOW] ", "state snapshot sent reason=%s",
                    reason != NULL ? reason : "");
    } else {
        safety_logf("[FLOW] ", "state snapshot send failed reason=%s rc=%d",
                    reason != NULL ? reason : "",
                    rc);
    }
}

static int safety_collect_pre_start_baseline(settlement_mode_t settlement)
{
    runtime_state_t *rs = runtime_state_mutable();
    meter_snapshot_t meter;
    flow_snapshot_t flow;

    safety_set_main_state(SESSION_STATE_PRE_START_METERING);
    runtime_state_set_last_event(EVT_READY_CHECK_OK);

    if (safety_meter_read_snapshot(&meter) == 0) {
        runtime_state_set_meter_snapshots(&meter, &meter, NULL);
        rs->voltage_v = meter.voltage_v;
        rs->current_a = meter.current_a;
        rs->power_kw = meter.power_kw;
        rs->energy_kwh = meter.energy_kwh;
    } else if (safety_meter_required(settlement)) {
        runtime_state_set_last_event(EVT_PRE_START_METERING_FAIL);
        safety_voice_prompt(VOICE_METER_QUERY_FAILED);
        return SAFETY_FLOW_ERR_METER;
    }
    if (safety_flow_read_snapshot(&flow) == 0) {
        runtime_state_set_flow_snapshots(&flow, &flow, NULL);
        rs->flow_m3h = flow.flow_m3h;
        rs->total_m3 = flow.total_m3;
    } else if (safety_flow_required(settlement)) {
        runtime_state_set_last_event(EVT_PRE_START_METERING_FAIL);
        safety_voice_prompt(VOICE_METER_QUERY_FAILED);
        return SAFETY_FLOW_ERR_METER;
    }

    runtime_state_set_last_event(EVT_PRE_START_METERING_OK);
    return 0;
}

static int safety_execute_start_sequence(session_target_type_t target, linkage_mode_t linkage)
{
    const protection_config_t *pc = config_store_protection();

    if (linkage == LINKAGE_PLATFORM_ORCHESTRATED && target == TARGET_PUMP_WITH_VALVE) {
        return SAFETY_FLOW_ERR_UNSUPPORTED_LINK;
    }

    safety_set_main_state(SESSION_STATE_START_SEQUENCE);
    runtime_state_set_run_state(RUNTIME_RUN_STARTING);

    if (safety_target_has_valve(target)) {
        safety_voice_prompt(VOICE_STARTING_VALVE);
        if (safety_run_local_action("single_valve_control", "open_valve") != 0U) {
            runtime_state_set_last_event(EVT_START_SEQUENCE_FAIL);
            return SAFETY_FLOW_ERR_SEQUENCE;
        }
        runtime_state_set_valve_state(RUNTIME_VALVE_OPEN);
        safety_maybe_delay_ms(pc != NULL ? pc->start_delay_ms : 0U, "start_after_open_valve");
    }
    if (safety_target_has_pump(target)) {
        safety_voice_prompt(VOICE_STARTING_PUMP);
        if (safety_run_pump_action(1U) != 0U) {
            runtime_state_set_last_event(EVT_START_SEQUENCE_FAIL);
            return SAFETY_FLOW_ERR_SEQUENCE;
        }
        runtime_state_set_pump_state(RUNTIME_PUMP_RUNNING);
    }

    runtime_state_set_last_event(EVT_START_SEQUENCE_OK);
    return 0;
}

static void safety_fill_detail_json(char *detail_json, size_t detail_cap, const char *action_code, const char *result)
{
    const runtime_state_t *rs = runtime_state_get();

    if (detail_json == NULL || detail_cap == 0U) {
        return;
    }
    (void)snprintf(detail_json, detail_cap,
                   "\"action_code\":\"%s\",\"result\":\"%s\",\"session_ref\":\"%s\","
                   "\"target_type\":\"%s\",\"linkage_mode\":\"%s\","
                   "\"workflow_state\":\"%s\",\"run_state\":\"%s\"",
                   action_code != NULL ? action_code : "",
                   result != NULL ? result : "",
                   rs->session_ref,
                   runtime_state_target_name(rs->target_type),
                   runtime_state_linkage_name(rs->linkage_mode),
                   runtime_state_workflow_name(rs->workflow_state),
                   runtime_state_run_name(rs->run_state));
}

static int safety_start_session_internal(const char *session_ref,
                                         session_target_type_t target,
                                         linkage_mode_t linkage,
                                         settlement_mode_t settlement,
                                         uint16_t lease_sec,
                                         uint8_t keepalive_required,
                                         uint8_t allow_recovery_override)
{
    runtime_state_t *rs = runtime_state_mutable();
    blocked_reason_code_t blocked = BLOCKED_NONE;
    char resolved_session_ref[CTRL_SESSION_REF_LEN];
    int rc;

    if (safety_ready_check(0U, settlement, allow_recovery_override, &blocked) != 0) {
        safety_apply_blocked(blocked);
        return SAFETY_FLOW_ERR_BLOCKED;
    }

    memset(resolved_session_ref, 0, sizeof(resolved_session_ref));
    if (session_ref != NULL && session_ref[0] != '\0') {
        safety_copy_string(resolved_session_ref, sizeof(resolved_session_ref), session_ref);
    } else {
        safety_generate_session_ref(resolved_session_ref, sizeof(resolved_session_ref), "sess");
    }

    runtime_state_set_blocked_reason(BLOCKED_NONE);
    runtime_state_set_session_ref(resolved_session_ref);
    runtime_state_set_session_modes(target, linkage, settlement);
    runtime_state_set_runtime_sec(0U);
    runtime_state_set_stop_reason(STOP_REASON_NONE);
    rs->summary.finalized = 0U;
    s_flow_ctx.started_ms = s_flow_ctx.last_tick_ms;

    rc = safety_collect_pre_start_baseline(settlement);
    if (rc != 0) {
        safety_set_main_state(SESSION_STATE_BLOCKED);
        runtime_state_set_run_state(RUNTIME_RUN_STANDBY);
        return rc;
    }

    rc = safety_execute_start_sequence(target, linkage);
    if (rc != 0) {
        safety_voice_prompt(VOICE_START_FAILED);
        safety_set_main_state(SESSION_STATE_BLOCKED);
        runtime_state_set_run_state(RUNTIME_RUN_STANDBY);
        return rc;
    }

    runtime_state_set_run_state(RUNTIME_RUN_RUNNING);
    safety_set_main_state(SESSION_STATE_RUNNING);
    safety_set_safe_state(SAFE_STATE_NORMAL);
    runtime_state_inc_counter_session_start();
    if (lease_sec == 0U) {
        lease_sec = SAFETY_DEFAULT_LEASE_SEC;
    }
    safety_set_lease(resolved_session_ref, lease_sec, keepalive_required);
    if (s_last_authorized_card_no[0] != '\0') {
        workflow_local_access_note_start_accepted(s_last_authorized_card_no, s_flow_ctx.last_tick_ms);
        safety_clear_last_authorized_card();
    }
    safety_voice_prompt(VOICE_START_SUCCESS);
    safety_send_state_snapshot_now("session_started");
    safety_update_summary(false);
    safety_persist_runtime(true, false);
    safety_logf("[FLOW] ", "session started ref=%s target=%s linkage=%s",
                resolved_session_ref,
                runtime_state_target_name(target),
                runtime_state_linkage_name(linkage));
    return 0;
}

static int safety_collect_post_stop_metering(settlement_mode_t settlement, bool *finalized)
{
    meter_snapshot_t meter;
    flow_snapshot_t flow;

    if (finalized != NULL) {
        *finalized = true;
    }

    safety_set_main_state(SESSION_STATE_POST_STOP_METERING);
    if (safety_meter_read_snapshot(&meter) == 0) {
        runtime_state_set_meter_snapshots(NULL, &meter, &meter);
    } else if (safety_meter_required(settlement)) {
        if (finalized != NULL) {
            *finalized = false;
        }
    }
    if (safety_flow_read_snapshot(&flow) == 0) {
        runtime_state_set_flow_snapshots(NULL, &flow, &flow);
    } else if (safety_flow_required(settlement)) {
        if (finalized != NULL) {
            *finalized = false;
        }
    }
    runtime_state_set_last_event(EVT_POST_STOP_METERING_OK);
    return 0;
}

static void safety_emit_session_event(const char *event_code)
{
    const runtime_state_t *rs = runtime_state_get();
    const char *short_code = "ss";
    const char *target_ref = NULL;
    const char *detail = NULL;

    if (rs == NULL) {
        return;
    }
    if (event_code != NULL && strcmp(event_code, "abnormal_power_interrupt") == 0) {
        short_code = "api";
    }
    if (rs->summary.target_type == TARGET_PUMP || rs->summary.target_type == TARGET_PUMP_WITH_VALVE) {
        target_ref = "pump_1";
    } else if (rs->summary.target_type == TARGET_VALVE) {
        target_ref = "valve_1";
    }
    if (rs->summary.finalized == 0U) {
        detail = "unsettled";
    }
    if (proto_event_report_send_min(rs->session_ref[0] != '\0' ? rs->session_ref : NULL,
                                    short_code,
                                    runtime_state_stop_reason_name(rs->summary.stop_reason),
                                    detail,
                                    target_ref) > 0) {
        runtime_state_inc_counter_event_report();
    } else {
        safety_queue_event_report(rs->session_ref,
                                  short_code,
                                  runtime_state_stop_reason_name(rs->summary.stop_reason),
                                  detail,
                                  target_ref);
    }
}

static int safety_close_local_outputs(session_target_type_t target, linkage_mode_t linkage)
{
    const protection_config_t *pc = config_store_protection();

    if (safety_target_has_pump(target)) {
        if (safety_run_pump_action(0U) != 0U) {
            return SAFETY_FLOW_ERR_SEQUENCE;
        }
        runtime_state_set_pump_state(RUNTIME_PUMP_STOPPED);
        if (safety_target_has_valve(target)) {
            safety_maybe_delay_ms(pc != NULL ? pc->stop_delay_ms : 0U, "stop_before_close_valve");
        }
    }
    if (safety_target_has_valve(target) &&
        (linkage == LINKAGE_LOCAL_INTEGRATED || target == TARGET_VALVE || target == TARGET_PUMP_WITH_VALVE)) {
        if (safety_run_local_action("single_valve_control", "close_valve") != 0U) {
            return SAFETY_FLOW_ERR_SEQUENCE;
        }
        runtime_state_set_valve_state(RUNTIME_VALVE_CLOSED);
    }
    return 0;
}

static int safety_stop_session_internal(session_stop_reason_t reason, bool abnormal, bool recovery_locked)
{
    runtime_state_t *rs = runtime_state_mutable();
    bool finalized = true;

    if (!safety_session_active(rs) && rs->workflow_state != RUNTIME_WORKFLOW_AUTH_PENDING) {
        return SAFETY_FLOW_ERR_NOT_RUNNING;
    }

    runtime_state_set_stop_reason(reason);
    safety_set_main_state(SESSION_STATE_STOP_SEQUENCE);
    runtime_state_set_run_state(RUNTIME_RUN_STOPPING);
    runtime_state_set_last_event(EVT_STOP_REQUEST);
    safety_set_safe_state(abnormal ? SAFE_STATE_STOPPING : SAFE_STATE_NORMAL);
    safety_voice_prompt(reason == STOP_REASON_PUMP_POWER_LOSS ? VOICE_POWER_INTERRUPTED : VOICE_STOPPING);

    if (safety_close_local_outputs(rs->target_type, rs->linkage_mode) != 0) {
        runtime_state_set_last_event(EVT_STOP_REQUEST);
        if (safety_target_has_pump(rs->target_type)) {
            safety_set_main_state(SESSION_STATE_RECOVERY_LOCKED);
            runtime_state_set_run_state(RUNTIME_RUN_RECOVERY_LOCKED);
            runtime_state_set_blocked_reason(BLOCKED_RECOVERY_LOCKED);
            safety_set_safe_state(SAFE_STATE_LATCHED);
            rs->recovery_event_pending = 1U;
            safety_schedule_recovery_safe_off(SAFETY_RECOVERY_RETRY_MS);
            safety_persist_runtime(true, false);
            safety_voice_prompt(VOICE_RECOVERY_LOCKED_CONFIRM_REQUIRED);
            safety_logf("[SAFE] ", "stop uncertain, recovery locked session=%s reason=%s",
                        rs->session_ref,
                        runtime_state_stop_reason_name(reason));
        } else {
            runtime_state_set_run_state(RUNTIME_RUN_RUNNING);
            if (abnormal) {
                safety_set_main_state(SESSION_STATE_FAULT_LATCHED);
                runtime_state_set_run_state(RUNTIME_RUN_FAULT_LATCHED);
                safety_set_safe_state(SAFE_STATE_LATCHED);
            } else {
                safety_set_main_state(SESSION_STATE_RUNNING);
                safety_set_safe_state(SAFE_STATE_NORMAL);
            }
        }
        return SAFETY_FLOW_ERR_SEQUENCE;
    }
    runtime_state_set_last_event(EVT_STOP_SEQUENCE_OK);

    (void)safety_collect_post_stop_metering(rs->settlement_mode, &finalized);
    if (reason == STOP_REASON_CONTROLLER_POWER_INTERRUPT) {
        finalized = false;
    }
    safety_update_summary(finalized);
    safety_emit_session_event(reason == STOP_REASON_CONTROLLER_POWER_INTERRUPT ? "abnormal_power_interrupt"
                                                                               : "session_stop");
    runtime_state_inc_counter_session_stop();
    safety_clear_lease();
    workflow_local_access_note_stop_accepted(s_flow_ctx.last_tick_ms);
    safety_clear_last_authorized_card();

    if (recovery_locked) {
        safety_set_main_state(SESSION_STATE_RECOVERY_LOCKED);
        runtime_state_set_run_state(RUNTIME_RUN_RECOVERY_LOCKED);
        runtime_state_set_blocked_reason(BLOCKED_RECOVERY_LOCKED);
        safety_voice_prompt(VOICE_RECOVERY_LOCKED_CONFIRM_REQUIRED);
        rs->recovery_event_pending = 1U;
    } else if (abnormal) {
        safety_set_main_state(SESSION_STATE_FAULT_LATCHED);
        runtime_state_set_run_state(RUNTIME_RUN_FAULT_LATCHED);
    } else {
        safety_set_main_state(SESSION_STATE_READY_IDLE);
        runtime_state_set_run_state(RUNTIME_RUN_STANDBY);
        runtime_state_set_blocked_reason(BLOCKED_NONE);
    }

    safety_set_card_state(CARD_STATE_IDLE);
    safety_set_safe_state((abnormal || recovery_locked) ? SAFE_STATE_LATCHED : SAFE_STATE_NORMAL);
    if (recovery_locked) {
        safety_schedule_recovery_safe_off(0U);
    } else {
        safety_voice_prompt(abnormal ? VOICE_PROTECTION_TRIGGERED : VOICE_STOPPED);
    }
    safety_persist_runtime(recovery_locked, finalized);
    return 0;
}

static void safety_refresh_recovery_event(void)
{
    runtime_state_t *rs = runtime_state_mutable();
    const common_status_t *cs = common_status_get();

    if (rs->recovery_event_pending == 0U || cs == NULL || !cs->online || !cs->tcp_connected) {
        return;
    }

    if (proto_event_report_send_min(rs->session_ref[0] != '\0' ? rs->session_ref : NULL,
                                    "api",
                                    "controller_power_interrupt",
                                    rs->power_domains.power_loss_high_risk != 0U ? "high_risk" : NULL,
                                    "pump_1") > 0) {
        rs->recovery_event_pending = 0U;
        runtime_state_inc_counter_event_report();
    }
}

static int safety_send_card_auth_query(void)
{
    json_buf_t jb;
    char message[640];
    int rc;
    session_target_type_t target = safety_default_target();
    char swipe_at[40];
    uint8_t have_swipe_at = 0U;

    json_buf_init(&jb, message, sizeof(message));
    if (proto_envelope_append_payload_prefix(&jb, PROTO_MSG_QUERY, 0U, NULL, s_card_ctx.pending_session_ref) != 0) {
        return SAFETY_FLOW_ERR_QUERY_SEND;
    }
    if (bsp_rtc_now_iso8601_utc(swipe_at, sizeof(swipe_at)) == 0) {
        have_swipe_at = 1U;
    }
    if (json_buf_append(&jb, "\"scope\":\"farmer_checkout\",\"query_code\":\"card_swipe\",\"entry_type\":\"card\",\"card_token\":\"") != 0 ||
        json_escape_append(&jb, s_card_ctx.card_no) != 0 ||
        json_buf_append(&jb, "\",\"swipe_action\":\"start\",\"swipe_event_id\":\"") != 0 ||
        json_escape_append(&jb, s_card_ctx.pending_session_ref) != 0 ||
        json_buf_append(&jb, "\",\"target_type\":\"") != 0 ||
        json_buf_append(&jb, runtime_state_target_name(target)) != 0 ||
        json_buf_append(&jb, "\"") != 0) {
        return SAFETY_FLOW_ERR_QUERY_SEND;
    }
    if (have_swipe_at != 0U) {
        if (json_buf_append(&jb, ",\"swipe_at\":\"") != 0 ||
            json_escape_append(&jb, swipe_at) != 0 ||
            json_buf_append(&jb, "\"") != 0) {
            return SAFETY_FLOW_ERR_QUERY_SEND;
        }
    }
    if (json_buf_append_fmt(&jb,
                            ",\"resource_lock\":\"%s\",\"timeout_ms\":%lu,"
                            "\"conflicts_with\":\"%s\",\"interruptible\":%s",
                            s_action_cloud_auth.resource_lock,
                            (unsigned long)s_action_cloud_auth.timeout_ms,
                            s_action_cloud_auth.conflicts_with,
                            s_action_cloud_auth.interruptible != 0U ? "true" : "false") != 0 ||
        proto_envelope_close_payload(&jb) != 0) {
        return SAFETY_FLOW_ERR_QUERY_SEND;
    }

    rc = net_connectivity_send_json(message, jb.len);
    if (rc >= 0) {
        safety_logf("[FLOW] ", "card swipe request sent token=%s session=%s target=%s",
                    s_card_ctx.card_no,
                    s_card_ctx.pending_session_ref,
                    runtime_state_target_name(target));
    }
    return rc >= 0 ? 0 : SAFETY_FLOW_ERR_QUERY_SEND;
}

static void safety_load_recovery_lock_from_persist(void)
{
    device_runtime_t runtime;
    runtime_state_t *rs = runtime_state_mutable();

    memset(&runtime, 0, sizeof(runtime));
    if (storage_runtime_load(&runtime) != 0) {
        return;
    }
    if (runtime.power_loss_persist.magic != SAFETY_PERSIST_MAGIC) {
        return;
    }
    if (runtime.power_loss_persist.last_session_active == 0U) {
        return;
    }

    runtime_state_set_session_ref(runtime.power_loss_persist.session_ref);
    runtime_state_set_session_modes(runtime.power_loss_persist.last_target_type,
                                    runtime.power_loss_persist.last_linkage_mode,
                                    safety_default_settlement());
    runtime_state_set_stop_reason(STOP_REASON_CONTROLLER_POWER_INTERRUPT);
    runtime_state_set_power_domain_state(POWER_DOMAIN_ON,
                                         POWER_DOMAIN_UNKNOWN,
                                         POWER_DOMAIN_UNKNOWN,
                                         POWER_DOMAIN_UNKNOWN,
                                         runtime.power_loss_persist.power_loss_high_risk);
    safety_set_main_state(SESSION_STATE_RECOVERY_LOCKED);
    runtime_state_set_run_state(RUNTIME_RUN_RECOVERY_LOCKED);
    runtime_state_set_blocked_reason(BLOCKED_RECOVERY_LOCKED);
    runtime_state_set_runtime_sec(runtime.power_loss_persist.runtime_sec);
    rs->energy_kwh = runtime.power_loss_persist.last_energy_kwh;
    rs->total_m3 = runtime.power_loss_persist.last_total_m3;
    rs->recovery_event_pending = 1U;
    safety_schedule_recovery_safe_off(0U);
    safety_voice_prompt(VOICE_POWER_RESTORED_SELF_CHECK);
    safety_voice_prompt(VOICE_RECOVERY_LOCKED_CONFIRM_REQUIRED);
    safety_logf("[POWER] ", "boot recovery locked session=%s high_risk=%u",
                runtime.power_loss_persist.session_ref,
                (unsigned)runtime.power_loss_persist.power_loss_high_risk);
}

void safety_flow_init(void)
{
    memset(&s_card_ctx, 0, sizeof(s_card_ctx));
    memset(&s_flow_ctx, 0, sizeof(s_flow_ctx));
    memset(&s_card_report, 0, sizeof(s_card_report));
    memset(&s_pending_event_report, 0, sizeof(s_pending_event_report));
    memset(&s_counter_reset_report, 0, sizeof(s_counter_reset_report));
    memset(s_last_authorized_card_no, 0, sizeof(s_last_authorized_card_no));

    runtime_state_set_blocked_reason(BLOCKED_NONE);
    runtime_state_set_last_event(EVT_BOOT_OK);
    runtime_state_set_time_synced(bsp_rtc_is_synced() != 0);
    runtime_state_mutable()->boot_ok = 1U;
    runtime_state_set_power_domain_state(POWER_DOMAIN_ON,
                                         POWER_DOMAIN_UNKNOWN,
                                         POWER_DOMAIN_UNKNOWN,
                                         POWER_DOMAIN_UNKNOWN,
                                         0U);
    safety_set_card_state(CARD_STATE_IDLE);
    safety_set_safe_state(SAFE_STATE_NORMAL);
    safety_set_main_state(SESSION_STATE_BOOTING);
    runtime_state_set_run_state(RUNTIME_RUN_STANDBY);
    safety_bootstrap_meter_epoch();
    safety_load_recovery_lock_from_persist();
}

void safety_flow_poll_ready(void)
{
    const runtime_state_t *rs = runtime_state_get();
    const device_config_t *cfg = config_store_active();
    bool ready;

    ready = (cfg != NULL) &&
            (rs->boot_ok != 0U) &&
            (rs->protection_active == false) &&
            (rs->workflow_state != RUNTIME_WORKFLOW_RECOVERY_LOCKED) &&
            (rs->workflow_state != RUNTIME_WORKFLOW_FAULT_LATCHED) &&
            (safety_session_active(rs) == false);

    common_status_set_ready(ready);
    if (ready) {
        if (rs->workflow_state == RUNTIME_WORKFLOW_BOOTING ||
            rs->workflow_state == RUNTIME_WORKFLOW_NOT_READY ||
            rs->workflow_state == RUNTIME_WORKFLOW_BLOCKED) {
            safety_set_main_state(SESSION_STATE_READY_IDLE);
        }
    } else if (rs->workflow_state == RUNTIME_WORKFLOW_BOOTING) {
        safety_set_main_state(SESSION_STATE_NOT_READY);
    }
}

void safety_flow_tick(uint32_t monotonic_ms)
{
    runtime_state_t *rs = runtime_state_mutable();
    const device_config_t *cfg = config_store_active();
    const common_status_t *cs = common_status_get();

    if (s_flow_ctx.last_tick_ms != 0U && rs->run_state == RUNTIME_RUN_RUNNING) {
        rs->runtime_sec += (monotonic_ms - s_flow_ctx.last_tick_ms) / 1000U;
    }
    s_flow_ctx.last_tick_ms = monotonic_ms;

    safety_flow_poll_ready();
    safety_track_meter_epoch_events();
    safety_try_emit_card_auth_report();
    safety_try_emit_pending_event_report();
    safety_try_emit_counter_reset_report();
    safety_try_recovery_safe_off();

    if (rs->run_state == RUNTIME_RUN_RUNNING && rs->lease.keepalive_required != 0U &&
        rs->lease.lease_expire_at != 0U && safety_now_sec() >= rs->lease.lease_expire_at) {
        runtime_state_set_last_event(EVT_SESSION_LEASE_EXPIRED);
        runtime_state_set_stop_reason(STOP_REASON_SESSION_LEASE_EXPIRED);
        (void)safety_stop_session_internal(STOP_REASON_SESSION_LEASE_EXPIRED, true, true);
    }

    if (cfg != NULL && rs->run_state == RUNTIME_RUN_RUNNING && cs != NULL && !cs->online) {
        if (s_flow_ctx.offline_since_ms == 0U) {
            s_flow_ctx.offline_since_ms = monotonic_ms;
            runtime_state_set_last_event(EVT_NETWORK_LOST);
        }
        if (cfg->control_config.offline_max_runtime_sec > 0U &&
            (uint32_t)(monotonic_ms - s_flow_ctx.offline_since_ms) >=
                ((uint32_t)cfg->control_config.offline_max_runtime_sec * 1000U)) {
            (void)safety_stop_session_internal(STOP_REASON_SESSION_LEASE_EXPIRED, true, true);
        }
    } else {
        if (s_flow_ctx.offline_since_ms != 0U) {
            runtime_state_set_last_event(EVT_NETWORK_RECOVERED);
        }
        s_flow_ctx.offline_since_ms = 0U;
    }

    if (rs->run_state == RUNTIME_RUN_RUNNING &&
        safety_target_has_pump(rs->target_type) &&
        rs->power_domains.controller_power_domain == POWER_DOMAIN_ON &&
        rs->power_domains.pump_power_domain == POWER_DOMAIN_LOST) {
        runtime_state_inc_counter_power_loss();
        runtime_state_set_last_event(EVT_PUMP_POWER_LOST);
        (void)safety_stop_session_internal(STOP_REASON_PUMP_POWER_LOSS, true, true);
    }

    safety_refresh_recovery_event();

    switch (s_card_ctx.state) {
    case CARD_STATE_IDLE:
    case CARD_STATE_COMPLETED:
    case CARD_STATE_BLOCKED:
        if (s_card_ctx.state != CARD_STATE_IDLE &&
            (uint32_t)(monotonic_ms - s_card_ctx.state_since_ms) >= SAFETY_CARD_RESULT_HOLD_MS) {
            memset(&s_card_ctx, 0, sizeof(s_card_ctx));
            safety_set_card_state(CARD_STATE_IDLE);
        }
        break;
    case CARD_STATE_READ:
        safety_set_card_state(CARD_STATE_DEBOUNCE);
        break;
    case CARD_STATE_DEBOUNCE:
        if ((uint32_t)(monotonic_ms - s_card_ctx.state_since_ms) >= SAFETY_CARD_DEBOUNCE_MS) {
            blocked_reason_code_t blocked = BLOCKED_NONE;
            safety_set_card_state(CARD_STATE_READY_CHECK);
            if (safety_ready_check(1U, safety_default_settlement(), 0U, &blocked) != 0) {
                safety_apply_blocked(blocked);
                safety_set_card_state(CARD_STATE_BLOCKED);
            } else {
                safety_voice_prompt(VOICE_AUTH_CHECKING);
                safety_set_main_state(SESSION_STATE_AUTH_PENDING);
                safety_set_card_state(CARD_STATE_CLOUD_AUTH_PENDING);
                s_card_ctx.auth_timeout_ms = SAFETY_CARD_AUTH_TIMEOUT_MS;
                if (safety_send_card_auth_query() != 0) {
                    safety_apply_blocked(BLOCKED_NETWORK_UNAVAILABLE);
                    safety_set_card_state(CARD_STATE_BLOCKED);
                }
            }
        }
        break;
    case CARD_STATE_CLOUD_AUTH_PENDING:
        if (cs == NULL || !cs->online || !cs->tcp_connected) {
            runtime_state_set_last_event(EVT_CLOUD_AUTH_DENIED);
            safety_logf("[FLOW] ", "card swipe interrupted token=%s session=%s",
                        s_card_ctx.card_no,
                        s_card_ctx.pending_session_ref);
            safety_queue_card_auth_report("connection_interrupted_pending",
                                          "tcp_disconnected_before_platform_result");
            safety_voice_prompt(VOICE_AUTH_DENIED);
            safety_set_main_state(SESSION_STATE_BLOCKED);
            safety_set_card_state(CARD_STATE_AUTH_DENIED);
        } else if ((uint32_t)(monotonic_ms - s_card_ctx.state_since_ms) >= s_card_ctx.auth_timeout_ms) {
            runtime_state_set_last_event(EVT_CLOUD_AUTH_DENIED);
            safety_logf("[FLOW] ", "card swipe auth timeout token=%s session=%s timeout_ms=%lu",
                        s_card_ctx.card_no,
                        s_card_ctx.pending_session_ref,
                        (unsigned long)s_card_ctx.auth_timeout_ms);
            safety_queue_card_auth_report("local_auth_timeout", "platform_result_timeout");
            safety_voice_prompt(VOICE_AUTH_DENIED);
            safety_set_main_state(SESSION_STATE_BLOCKED);
            safety_set_card_state(CARD_STATE_AUTH_DENIED);
        }
        break;
    case CARD_STATE_AUTH_GRANTED:
        safety_set_card_state(CARD_STATE_COMPLETED);
        break;
    case CARD_STATE_AUTH_DENIED:
        safety_set_card_state(CARD_STATE_COMPLETED);
        break;
    case CARD_STATE_STARTING:
        safety_set_card_state(CARD_STATE_COMPLETED);
        break;
    case CARD_STATE_READY_CHECK:
    case CARD_STATE_VOICE_PROMPT:
    default:
        break;
    }
}

void safety_flow_poll_protection(uint32_t monotonic_ms)
{
    const protection_config_t *pc = config_store_protection();
    const runtime_state_t *rs = runtime_state_get();

    (void)monotonic_ms;
    if (pc == NULL || rs == NULL || rs->run_state != RUNTIME_RUN_RUNNING) {
        return;
    }

    if (pc->overload_protection != 0U &&
        pc->over_current_limit_a > 0.0f &&
        rs->current_a > pc->over_current_limit_a) {
        runtime_state_set_protection(RUNTIME_PROTECT_OVERLOAD, true);
    } else if (pc->phase_loss_protection != 0U &&
               rs->meter_last.valid != 0U &&
               rs->meter_last.phase_status == PHASE_STATUS_LOSS) {
        runtime_state_set_protection(RUNTIME_PROTECT_PHASE_LOSS, true);
    } else if (pc->under_voltage_protection != 0U &&
               pc->under_voltage_limit_v > 0.0f &&
               rs->voltage_v > 0.0f &&
               rs->voltage_v < pc->under_voltage_limit_v) {
        runtime_state_set_protection(RUNTIME_PROTECT_UNDER_VOLTAGE, true);
    } else if (pc->over_voltage_protection != 0U &&
               pc->over_voltage_limit_v > 0.0f &&
               rs->voltage_v > pc->over_voltage_limit_v) {
        runtime_state_set_protection(RUNTIME_PROTECT_OVER_VOLTAGE, true);
    } else if (pc->dry_run_protection != 0U &&
               rs->pressure_mpa < pc->pressure_low_limit &&
               rs->flow_m3h < 0.05f) {
        runtime_state_set_protection(RUNTIME_PROTECT_DRY_RUN, true);
    } else if (pc->pressure_high_limit > 0.0f && rs->pressure_mpa > pc->pressure_high_limit) {
        runtime_state_set_protection(RUNTIME_PROTECT_PRESSURE_HIGH, true);
    } else if (pc->pressure_low_limit > 0.0f && rs->pressure_mpa < pc->pressure_low_limit) {
        runtime_state_set_protection(RUNTIME_PROTECT_PRESSURE_LOW, true);
    }

    if (runtime_state_get()->protection_active) {
        runtime_state_inc_counter_protection_trip();
        runtime_state_set_last_event(EVT_PROTECTION_TRIGGERED);
        (void)safety_stop_session_internal(STOP_REASON_PROTECTION_TRIP, true, false);
    }
}

int safety_flow_on_card_read(const char *card_no)
{
    if (card_no == NULL || card_no[0] == '\0') {
        return SAFETY_FLOW_ERR_INVALID_ACTION;
    }
    if (s_card_ctx.state != CARD_STATE_IDLE &&
        s_card_ctx.state != CARD_STATE_COMPLETED &&
        s_card_ctx.state != CARD_STATE_BLOCKED) {
        return SAFETY_FLOW_ERR_BUSY;
    }

    memset(&s_card_ctx, 0, sizeof(s_card_ctx));
    (void)strncpy(s_card_ctx.card_no, card_no, sizeof(s_card_ctx.card_no) - 1U);
    safety_generate_session_ref(s_card_ctx.pending_session_ref, sizeof(s_card_ctx.pending_session_ref), "card");
    runtime_state_set_last_event(EVT_CARD_SWIPED);
    safety_set_card_state(CARD_STATE_READ);
    safety_logf("[FLOW] ", "card read accepted token=%s", card_no);
    (void)s_action_card;
    return 0;
}

void safety_flow_on_query_result(const char *json)
{
    char result[40];
    char reject_reason[64];

    if (json == NULL || s_card_ctx.state != CARD_STATE_CLOUD_AUTH_PENDING) {
        return;
    }
    if (proto_json_get_string(json, "result", result, sizeof(result)) != 0 &&
        proto_json_get_string(json, "auth_result", result, sizeof(result)) != 0) {
        return;
    }
    reject_reason[0] = '\0';
    (void)proto_json_get_string(json, "reject_reason", reject_reason, sizeof(reject_reason));
    if (reject_reason[0] == '\0') {
        (void)proto_json_get_string(json, "auth_result_detail", reject_reason, sizeof(reject_reason));
    }

    if (strcmp(result, "allowed") == 0 || strcmp(result, "accepted") == 0 ||
        strcmp(result, "granted") == 0 || strcmp(result, "ok") == 0 ||
        strcmp(result, "success") == 0) {
        runtime_state_set_last_event(EVT_CLOUD_AUTH_GRANTED);
        safety_voice_prompt(VOICE_AUTH_GRANTED);
        safety_copy_symbol(s_last_authorized_card_no, sizeof(s_last_authorized_card_no), s_card_ctx.card_no);
        safety_logf("[FLOW] ", "card swipe accepted token=%s session=%s waiting_start_session=1",
                    s_card_ctx.card_no,
                    s_card_ctx.pending_session_ref);
        safety_set_card_state(CARD_STATE_AUTH_GRANTED);
    } else {
        runtime_state_set_last_event(EVT_CLOUD_AUTH_DENIED);
        safety_voice_prompt(VOICE_AUTH_DENIED);
        safety_logf("[FLOW] ", "card swipe denied token=%s session=%s",
                    s_card_ctx.card_no,
                    s_card_ctx.pending_session_ref);
        safety_queue_card_auth_report("platform_denied",
                                      reject_reason[0] != '\0' ? reject_reason : "platform_denied");
        safety_clear_last_authorized_card();
        safety_set_card_state(CARD_STATE_AUTH_DENIED);
    }
}

int safety_flow_execute_action(const char *action_code, const char *json,
                               char *detail_json, size_t detail_cap)
{
    runtime_state_t *rs = runtime_state_mutable();
    session_target_type_t target;
    linkage_mode_t linkage;
    settlement_mode_t settlement;
    char session_ref[CTRL_SESSION_REF_LEN];
    uint32_t lease_sec_u32 = SAFETY_DEFAULT_LEASE_SEC;
    uint8_t keepalive_required = 1U;
    uint8_t allow_recovery_override = 0U;
    int rc;

    if (action_code == NULL) {
        return SAFETY_FLOW_ERR_INVALID_ACTION;
    }

    session_ref[0] = '\0';
    if (json != NULL) {
        (void)proto_json_get_string(json, "session_ref", session_ref, sizeof(session_ref));
        (void)proto_json_get_u32(json, "session_lease_sec", &lease_sec_u32);
        (void)proto_json_get_u8_01(json, "keepalive_required", &keepalive_required);
        (void)proto_json_get_u8_01(json, "recovery_acknowledged", &allow_recovery_override);
    }

    if (strcmp(action_code, "voice_broadcast") == 0) {
        char prompt_code[40];

        if (json != NULL && proto_json_get_string(json, "prompt_code", prompt_code, sizeof(prompt_code)) == 0) {
            workflow_voice_prompt_once(prompt_code, "platform_voice", 0U);
            safety_logf("[VOICE] ", "broadcast=%s", prompt_code);
        }
        safety_fill_detail_json(detail_json, detail_cap, action_code, "accepted");
        return 0;
    }

    if (strcmp(action_code, "stop_session") == 0) {
        rc = safety_stop_session_internal(STOP_REASON_PLATFORM_STOP, false, false);
        if (rc == 0) {
            safety_fill_detail_json(detail_json, detail_cap, action_code, "accepted");
        }
        return rc;
    }
    if (strcmp(action_code, "stop_pump") == 0) {
        runtime_state_set_session_modes(TARGET_PUMP, rs->linkage_mode, rs->settlement_mode);
        rc = safety_stop_session_internal(STOP_REASON_PLATFORM_STOP, false, false);
        if (rc == 0) {
            safety_fill_detail_json(detail_json, detail_cap, action_code, "accepted");
        }
        return rc;
    }
    if (strcmp(action_code, "close_valve") == 0) {
        runtime_state_set_session_modes(TARGET_VALVE, rs->linkage_mode, rs->settlement_mode);
        rc = safety_stop_session_internal(STOP_REASON_PLATFORM_STOP, false, false);
        if (rc == 0) {
            safety_fill_detail_json(detail_json, detail_cap, action_code, "accepted");
        }
        return rc;
    }

    if (strcmp(action_code, "start_pump") == 0) {
        target = TARGET_PUMP;
        linkage = safety_default_linkage();
        settlement = safety_default_settlement();
    } else if (strcmp(action_code, "open_valve") == 0) {
        target = TARGET_VALVE;
        linkage = safety_default_linkage();
        settlement = safety_default_settlement();
    } else if (strcmp(action_code, "start_session") == 0) {
        if (safety_parse_target_type(json, &target) != 0 ||
            safety_parse_linkage_mode(json, &linkage) != 0 ||
            safety_parse_settlement_mode(json, &settlement) != 0) {
            return SAFETY_FLOW_ERR_INVALID_ACTION;
        }
    } else {
        return SAFETY_FLOW_ERR_INVALID_ACTION;
    }

    if (allow_recovery_override != 0U && rs->workflow_state == RUNTIME_WORKFLOW_RECOVERY_LOCKED) {
        runtime_state_set_blocked_reason(BLOCKED_NONE);
        safety_set_main_state(SESSION_STATE_NOT_READY);
        runtime_state_set_run_state(RUNTIME_RUN_STANDBY);
    }

    rc = safety_start_session_internal(session_ref[0] != '\0' ? session_ref : NULL,
                                       target,
                                       linkage,
                                       settlement,
                                       (uint16_t)(lease_sec_u32 > 0xFFFFU ? 0xFFFFU : lease_sec_u32),
                                       keepalive_required,
                                       allow_recovery_override);
    if (rc == 0) {
        safety_fill_detail_json(detail_json, detail_cap, action_code, "accepted");
    }
    return rc;
}

const char *safety_flow_error_message(int code)
{
    switch (code) {
    case SAFETY_FLOW_ERR_INVALID_ACTION: return "invalid action";
    case SAFETY_FLOW_ERR_BLOCKED: return runtime_state_blocked_reason_name(runtime_state_get()->blocked_reason);
    case SAFETY_FLOW_ERR_BUSY: return "flow busy";
    case SAFETY_FLOW_ERR_SEQUENCE: return "start sequence failed";
    case SAFETY_FLOW_ERR_METER: return "meter or flow baseline failed";
    case SAFETY_FLOW_ERR_QUERY_SEND: return "cloud auth request failed";
    case SAFETY_FLOW_ERR_AUTH_DENIED: return "cloud auth denied";
    case SAFETY_FLOW_ERR_NOT_RUNNING: return "no running session";
    case SAFETY_FLOW_ERR_UNSUPPORTED_LINK: return "cross-device linkage requires platform orchestration";
    default: return "safety flow rejected";
    }
}

int safety_flow_build_blocked_json(char *buf, size_t cap,
                                   const char *blocking_resource,
                                   const char *blocking_action,
                                   uint32_t retry_after_ms)
{
    json_buf_t jb;

    if (buf == NULL || cap < 96U) {
        return -1;
    }
    json_buf_init(&jb, buf, cap);
    if (json_buf_append(&jb, "{\"status\":\"blocked\",\"reason_code\":\"RESOURCE_BUSY\",\"blocking_resource\":\"") != 0 ||
        json_escape_append(&jb, blocking_resource != NULL ? blocking_resource : "") != 0 ||
        json_buf_append(&jb, "\",\"blocking_action\":\"") != 0 ||
        json_escape_append(&jb, blocking_action != NULL ? blocking_action : "") != 0 ||
        json_buf_append_fmt(&jb, "\",\"retry_after_ms\":%lu}", (unsigned long)retry_after_ms) != 0) {
        return -1;
    }
    return (int)jb.len;
}

const char *safety_flow_state_name(void)
{
    return runtime_state_card_name(runtime_state_get()->card_state);
}
