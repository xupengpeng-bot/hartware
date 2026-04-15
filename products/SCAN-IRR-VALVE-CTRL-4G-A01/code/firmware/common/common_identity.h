#ifndef COMMON_IDENTITY_H
#define COMMON_IDENTITY_H

#include "model_types.h"

void common_identity_init(void);

const controller_identity_t *common_identity_get(void);
controller_identity_t       *common_identity_mutable(void);

const resource_inventory_t *common_resource_inventory_get(void);
resource_inventory_t       *common_resource_inventory_mutable(void);

#endif /* COMMON_IDENTITY_H */
