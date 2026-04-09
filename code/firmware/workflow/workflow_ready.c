#include "workflow_ready.h"
#include "common_alarm.h"
#include "model_config.h"
#include "storage_config.h"

bool workflow_ready_key_modules_ok(void)
{
    uint32_t faults[COMMON_ALARM_MAX];
    size_t   n = common_alarm_copy(faults, COMMON_ALARM_MAX);
    if (n > 0U) {
        return false;
    }
    device_config_t cfg;
    if (storage_config_load(&cfg) != 0) {
        return false;
    }
    if (cfg.feature_modules.pump_vfd_control == 0U && cfg.feature_modules.single_valve_control == 0U) {
        return false;
    }
    return true;
}
