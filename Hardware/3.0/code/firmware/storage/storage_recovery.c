#include "storage_recovery.h"
#include <string.h>

static char s_hint[128];

void storage_recovery_init(void)
{
    s_hint[0] = '\0';
}

int storage_recovery_save_hint(const char *hint)
{
    if (!hint) {
        return -1;
    }
    (void)strncpy(s_hint, hint, sizeof(s_hint) - 1U);
    s_hint[sizeof(s_hint) - 1U] = '\0';
    return 0;
}

const char *storage_recovery_get_hint(void)
{
    return s_hint;
}
