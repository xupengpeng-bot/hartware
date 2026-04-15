/**
 * Board-level aggregated status for REGISTER / STATE_SNAPSHOT / QUERY common — Spec §7.
 */
#ifndef COMMON_STATUS_H
#define COMMON_STATUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define COMMON_FAULT_MAX 16U

typedef struct {
    bool     online;
    bool     tcp_connected;
    bool     registered_once;
    bool     ready;
    int16_t  signal_csq;
    int16_t  rsrp_dbm;
    int16_t  rsrq_db;
    uint8_t  battery_soc;
    float    battery_voltage_v;
    float    solar_voltage_v;
    uint8_t  power_mode;
    uint8_t  reboot_reason;
    uint32_t fault_codes[COMMON_FAULT_MAX];
    uint8_t  fault_count;
    uint32_t config_version;
    /** 大于 monotonic_ms 时，灯语 B 显示一次“心跳已发出”短闪 */
    uint32_t heartbeat_led_pulse_until_ms;
} common_status_t;

void common_status_init(void);

/** Spec §7.2 — call from 30 s tick. */
void common_status_refresh_slow(void);

void common_status_set_online(bool v);
void common_status_set_tcp_connected(bool v);
void common_status_set_registered(bool v);
void common_status_set_ready(bool v);
void common_status_set_config_version(uint32_t v);
void common_status_set_faults(const uint32_t *codes, size_t count);
void common_status_set_signal(int16_t csq, int16_t rsrp, int16_t rsrq);
void common_status_set_battery(uint8_t soc, float vbat, float vsolar);
void common_status_set_power_mode(uint8_t mode);
void common_status_set_reboot_reason(uint8_t reason);

const common_status_t *common_status_get(void);

/** 在成功发送 link_ping 心跳后调用，驱动状态灯短闪 */
void common_status_pulse_heartbeat_led(uint32_t monotonic_ms);

#endif /* COMMON_STATUS_H */
