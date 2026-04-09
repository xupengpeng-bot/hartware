#ifndef STORAGE_RUNTIME_H
#define STORAGE_RUNTIME_H

#include "model_runtime.h"
#include <stdbool.h>

void storage_runtime_init(void);

int storage_runtime_save(const device_runtime_t *rt);
int storage_runtime_load(device_runtime_t *out);

bool storage_runtime_has_dirty_session(void);
void storage_runtime_clear_dirty(void);

#endif /* STORAGE_RUNTIME_H */
