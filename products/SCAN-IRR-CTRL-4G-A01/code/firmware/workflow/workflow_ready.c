#include "workflow_ready.h"
#include "common_alarm.h"
#include "model_config.h"
#include "storage_config.h"

bool workflow_ready_key_modules_ok(void)
{
    uint32_t faults[COMMON_ALARM_MAX];
    size_t   n = common_alarm_copy(faults, COMMON_ALARM_MAX);
    const device_config_t *cfg;
    if (n > 0U) {
        return false;
    }
    cfg = storage_config_active();
    if (cfg == NULL) {
        return false;
    }
    if (cfg->feature_modules.breaker_control == 0U &&
        cfg->feature_modules.pump_direct_control == 0U &&
        cfg->feature_modules.pump_vfd_control == 0U) {
        return false;
    }
    return true;
}
