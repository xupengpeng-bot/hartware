#include "storage_config.h"
#include <string.h>

static device_config_t s_slot_a;
static device_config_t s_slot_b;
static bool            s_slot_a_valid;
static bool            s_slot_b_valid;
static uint8_t         s_active; /* 0 = A, 1 = B */

void storage_config_init(void)
{
    memset(&s_slot_a, 0, sizeof(s_slot_a));
    memset(&s_slot_b, 0, sizeof(s_slot_b));
    s_slot_a_valid = false;
    s_slot_b_valid = false;
    s_active = 0U;
}

bool storage_config_has_valid(void)
{
    return s_active == 0U ? s_slot_a_valid : s_slot_b_valid;
}

uint8_t storage_config_active_slot(void)
{
    return s_active;
}

int storage_config_load(device_config_t *out)
{
    if (!out) {
        return -1;
    }
    if (s_active == 0U) {
        if (!s_slot_a_valid) {
            return -2;
        }
        *out = s_slot_a;
        return 0;
    }
    if (!s_slot_b_valid) {
        return -2;
    }
    *out = s_slot_b;
    return 0;
}

int storage_config_stage_inactive(const device_config_t *cfg)
{
    if (!cfg) {
        return -1;
    }
    if (s_active == 0U) {
        s_slot_b = *cfg;
        s_slot_b_valid = true;
    } else {
        s_slot_a = *cfg;
        s_slot_a_valid = true;
    }
    return 0;
}

int storage_config_commit_swap(uint32_t new_version)
{
    if (s_active == 0U) {
        if (!s_slot_b_valid) {
            return -1;
        }
        s_slot_b.config_version = new_version;
        s_slot_b_valid = true;
        s_active = 1U;
    } else {
        if (!s_slot_a_valid) {
            return -1;
        }
        s_slot_a.config_version = new_version;
        s_slot_a_valid = true;
        s_active = 0U;
    }
    return 0;
}
