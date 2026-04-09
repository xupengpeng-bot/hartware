#include "common_identity.h"
#include <string.h>

static controller_identity_t s_id;
static resource_inventory_t  s_res;

void common_identity_init(void)
{
    memset(&s_id, 0, sizeof(s_id));
    memset(&s_res, 0, sizeof(s_res));
    (void)strncpy(s_id.firmware_family, "FW_H2_UNIFIED", sizeof(s_id.firmware_family) - 1U);
    (void)strncpy(s_id.firmware_version, "0.0.1", sizeof(s_id.firmware_version) - 1U);
    (void)strncpy(s_id.hardware_sku, "CTRL_UNIFIED", sizeof(s_id.hardware_sku) - 1U);
    (void)strncpy(s_id.hardware_rev, "A", sizeof(s_id.hardware_rev) - 1U);
}

const controller_identity_t *common_identity_get(void)
{
    return &s_id;
}

controller_identity_t *common_identity_mutable(void)
{
    return &s_id;
}

const resource_inventory_t *common_resource_inventory_get(void)
{
    return &s_res;
}

resource_inventory_t *common_resource_inventory_mutable(void)
{
    return &s_res;
}
