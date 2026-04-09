#include "app_scheduler.h"
#include "app_context.h"
#include "module_registry.h"
#include "workflow_card_reader.h"
#include "workflow_engine.h"
#include "workflow_voice.h"
#include "workflow_ready.h"
#include "common_status.h"
#include "storage_config.h"
#include "proto_state_snapshot.h"
#include "proto_heartbeat.h"
#include "net_connectivity.h"

#include <string.h>

/** 变化触发 vitals 的最小间隔，避免信号抖动刷爆流量（毫秒）。 */
#define VITALS_DELTA_MIN_GAP_MS 45000U

static uint32_t s_last_100ms;
static uint32_t s_last_1s;
static uint32_t s_last_30s;
static uint32_t s_last_snap;
static uint32_t s_last_ping;
static uint32_t s_last_vitals;
static uint32_t s_hb_seq;
static int16_t  s_last_sent_csq;
static uint8_t  s_last_sent_soc;
static uint8_t  s_vitals_baseline_ok;
static uint8_t  s_boot_vitals_sent;

static char s_sched_snap[1536];
static char s_sched_vitals[1024];

static void send_snapshot(void)
{
    int n = proto_state_snapshot_build(s_sched_snap, sizeof(s_sched_snap));
    if (n > 0) {
        (void)net_connectivity_send_json(s_sched_snap, (size_t)n);
    }
}

static void send_vitals_mark_baseline(uint32_t monotonic_ms)
{
    int n = proto_heartbeat_build_vitals(s_sched_vitals, sizeof(s_sched_vitals));
    if (n > 0) {
        if (net_connectivity_send_json(s_sched_vitals, (size_t)n) >= 0) {
            const common_status_t *st = common_status_get();
            s_last_sent_csq       = st->signal_csq;
            s_last_sent_soc       = st->battery_soc;
            s_vitals_baseline_ok  = 1U;
            s_last_vitals         = monotonic_ms;
        }
    }
}

static void try_delta_vitals(const runtime_rules_t *rules, uint32_t monotonic_ms)
{
    if (rules->link_ping_interval_sec == 0U || rules->vitals_interval_sec == 0U) {
        return;
    }
    if (!s_vitals_baseline_ok) {
        return;
    }
    if (monotonic_ms - s_last_vitals < VITALS_DELTA_MIN_GAP_MS) {
        return;
    }
    const common_status_t *st = common_status_get();
    if (rules->vitals_csq_delta > 0U) {
        int32_t diff = (int32_t)st->signal_csq - (int32_t)s_last_sent_csq;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= (int32_t)rules->vitals_csq_delta) {
            send_vitals_mark_baseline(monotonic_ms);
            return;
        }
    }
    if (rules->vitals_soc_delta > 0U) {
        uint32_t ds = st->battery_soc >= s_last_sent_soc ? (uint32_t)(st->battery_soc - s_last_sent_soc)
                                                           : (uint32_t)(s_last_sent_soc - st->battery_soc);
        if (ds >= (uint32_t)rules->vitals_soc_delta) {
            send_vitals_mark_baseline(monotonic_ms);
        }
    }
}

void app_scheduler_init(void)
{
    s_last_100ms         = 0U;
    s_last_1s            = 0U;
    s_last_30s           = 0U;
    s_last_snap          = 0U;
    s_last_ping          = 0U;
    s_last_vitals        = 0U;
    s_hb_seq             = 0U;
    s_last_sent_csq      = 0;
    s_last_sent_soc      = 0U;
    s_vitals_baseline_ok = 0U;
    s_boot_vitals_sent  = 0U;
}

void app_scheduler_tick(uint32_t monotonic_ms)
{
    app_context()->monotonic_ms = monotonic_ms;
    workflow_engine_tick_ms(monotonic_ms);
    workflow_card_reader_tick(monotonic_ms);
    workflow_voice_tick(monotonic_ms);

    if (monotonic_ms - s_last_100ms >= 100U) {
        s_last_100ms = monotonic_ms;
        module_registry_tick_100ms_all();
    }
    if (monotonic_ms - s_last_1s >= 1000U) {
        s_last_1s = monotonic_ms;
        module_registry_tick_1s_all();
        device_config_t cfg;
        bool loaded = (storage_config_load(&cfg) == 0);
        workflow_engine_poll_ready(loaded ? &cfg.runtime_rules : NULL, loaded,
                                   workflow_ready_key_modules_ok());
        if (loaded) {
            try_delta_vitals(&cfg.runtime_rules, monotonic_ms);
        }
    }
    if (monotonic_ms - s_last_30s >= 30000U) {
        s_last_30s = monotonic_ms;
        common_status_refresh_slow();
    }

    device_config_t cfg2;
    if (storage_config_load(&cfg2) != 0) {
        return;
    }

    const runtime_rules_t *rr = &cfg2.runtime_rules;
    const common_status_t *st = common_status_get();

    if (!st->registered_once) {
        return;
    }

    if (rr->snapshot_interval_sec > 0U) {
        uint32_t iv_snap = (uint32_t)rr->snapshot_interval_sec * 1000U;
        if (monotonic_ms - s_last_snap >= iv_snap) {
            s_last_snap = monotonic_ms;
            send_snapshot();
        }
    }

    /* 分层心跳：link_ping = 轻量保活；vitals = 体征；未配置 link_ping 时沿用单周期完整体征 */
    if (rr->link_ping_interval_sec > 0U) {
        if (rr->vitals_interval_sec > 0U && s_boot_vitals_sent == 0U && monotonic_ms >= 3000U) {
            s_boot_vitals_sent = 1U;
            send_vitals_mark_baseline(monotonic_ms);
        }
        uint32_t iv_ping = (uint32_t)rr->link_ping_interval_sec * 1000U;
        if (monotonic_ms - s_last_ping >= iv_ping) {
            s_last_ping = monotonic_ms;
            s_hb_seq++;
            char ping[768];
            uint32_t uptime_sec = monotonic_ms / 1000U;
            int      n          = proto_heartbeat_build_ping(ping, sizeof(ping), s_hb_seq, uptime_sec);
            if (n > 0) {
                int r = net_connectivity_send_json(ping, (size_t)n);
                if (r >= 0) {
                    common_status_pulse_heartbeat_led(monotonic_ms);
                }
            }
        }
        if (rr->vitals_interval_sec > 0U) {
            uint32_t iv_v = (uint32_t)rr->vitals_interval_sec * 1000U;
            if (monotonic_ms - s_last_vitals >= iv_v) {
                send_vitals_mark_baseline(monotonic_ms);
            }
        }
    } else if (rr->heartbeat_interval_sec > 0U) {
        uint32_t iv_hb = (uint32_t)rr->heartbeat_interval_sec * 1000U;
        if (monotonic_ms - s_last_ping >= iv_hb) {
            s_last_ping = monotonic_ms;
            send_vitals_mark_baseline(monotonic_ms);
        }
    }
}
