#include "common_status.h"
#include "bsp_adc.h"
#include "runtime_state.h"
#include <string.h>

static common_status_t s_st;

void common_status_init(void)
{
    memset(&s_st, 0, sizeof(s_st));
}

const common_status_t *common_status_get(void)
{
    return &s_st;
}

void common_status_set_online(bool v)
{
    s_st.online = v;
    runtime_state_set_online(v);
}

void common_status_set_tcp_connected(bool v)
{
    s_st.tcp_connected = v;
    runtime_state_set_tcp_connected(v);
}

void common_status_set_registered(bool v)
{
    s_st.registered_once = v;
    runtime_state_set_registered(v);
}

void common_status_set_ready(bool v)
{
    s_st.ready = v;
    runtime_state_set_ready(v);
}

void common_status_set_config_version(uint32_t v)
{
    s_st.config_version = v;
    runtime_state_set_config_version(v);
}

void common_status_refresh_slow(void)
{
    bsp_adc_sample_battery_to_status();
}

void common_status_set_faults(const uint32_t *codes, size_t count)
{
    if (!codes || count == 0U) {
        s_st.fault_count = 0U;
        memset(s_st.fault_codes, 0, sizeof(s_st.fault_codes));
        runtime_state_set_alarm_codes(NULL, 0U);
        return;
    }
    size_t n = count > COMMON_FAULT_MAX ? COMMON_FAULT_MAX : count;
    memcpy(s_st.fault_codes, codes, n * sizeof(uint32_t));
    s_st.fault_count = (uint8_t)n;
    runtime_state_set_alarm_codes(codes, n);
}

void common_status_set_signal(int16_t csq, int16_t rsrp, int16_t rsrq)
{
    s_st.signal_csq = csq;
    s_st.rsrp_dbm = rsrp;
    s_st.rsrq_db = rsrq;
    runtime_state_set_signal_csq(csq);
}

void common_status_set_battery(uint8_t soc, float vbat, float vsolar)
{
    s_st.battery_soc = soc;
    s_st.battery_voltage_v = vbat;
    s_st.solar_voltage_v = vsolar;
    runtime_state_set_battery_soc(soc);
}

void common_status_set_power_mode(uint8_t mode)
{
    s_st.power_mode = mode;
    runtime_state_set_power_state(mode == 1U ? RUNTIME_POWER_BATTERY :
                                  (mode == 2U ? RUNTIME_POWER_SOLAR : RUNTIME_POWER_MAINS));
}

void common_status_set_reboot_reason(uint8_t reason)
{
    s_st.reboot_reason = reason;
}

void common_status_pulse_heartbeat_led(uint32_t monotonic_ms)
{
    s_st.heartbeat_led_pulse_until_ms = monotonic_ms + 180U;
}
