#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "model_config.h"

void config_store_init(void);
void config_store_seed_defaults(void);

const device_config_t *config_store_active(void);
uint32_t config_store_config_version(void);
const protection_config_t *config_store_protection(void);
const control_config_t *config_store_control(void);

#endif /* CONFIG_STORE_H */
