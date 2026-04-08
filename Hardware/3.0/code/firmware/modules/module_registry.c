#include "module_registry.h"
#include "module_pump_vfd.h"
#include "module_single_valve.h"
#include "module_pressure.h"
#include "module_flow.h"
#include "module_meter.h"
#include "module_soil_moisture.h"
#include "module_soil_temperature.h"
#include <string.h>

static const module_ops_t *s_table[MODULE_REGISTRY_MAX];
static uint8_t             s_count;

void module_registry_init(void)
{
    memset(s_table, 0, sizeof(s_table));
    s_count = 0U;
}

uint8_t module_registry_register(const module_ops_t *ops)
{
    if (!ops || !ops->module_code || s_count >= MODULE_REGISTRY_MAX) {
        return 0xFFU;
    }
    s_table[s_count++] = ops;
    return (uint8_t)(s_count - 1U);
}

const module_ops_t *module_registry_get(const char *module_code)
{
    if (!module_code) {
        return NULL;
    }
    for (uint8_t i = 0U; i < s_count; i++) {
        if (s_table[i] && strcmp(s_table[i]->module_code, module_code) == 0) {
            return s_table[i];
        }
    }
    return NULL;
}

void module_registry_init_all(void)
{
    for (uint8_t i = 0U; i < s_count; i++) {
        if (s_table[i] && s_table[i]->init) {
            s_table[i]->init();
        }
    }
}

void module_registry_tick_100ms_all(void)
{
    for (uint8_t i = 0U; i < s_count; i++) {
        if (s_table[i] && s_table[i]->tick_100ms) {
            s_table[i]->tick_100ms();
        }
    }
}

void module_registry_tick_1s_all(void)
{
    for (uint8_t i = 0U; i < s_count; i++) {
        if (s_table[i] && s_table[i]->tick_1s) {
            s_table[i]->tick_1s();
        }
    }
}

void module_registry_register_builtin(void)
{
    (void)module_registry_register(module_pump_vfd_ops());
    (void)module_registry_register(module_single_valve_ops());
    (void)module_registry_register(module_pressure_ops());
    (void)module_registry_register(module_flow_ops());
    (void)module_registry_register(module_meter_ops());
    (void)module_registry_register(module_soil_moisture_ops());
    (void)module_registry_register(module_soil_temperature_ops());
}
