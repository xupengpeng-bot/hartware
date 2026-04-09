#ifndef STORAGE_RECOVERY_H
#define STORAGE_RECOVERY_H

#include <stdbool.h>

void storage_recovery_init(void);

/** Persist platform decision / context for recovery flow (RAM mirror). */
int storage_recovery_save_hint(const char *hint);
const char *storage_recovery_get_hint(void);

#endif /* STORAGE_RECOVERY_H */
