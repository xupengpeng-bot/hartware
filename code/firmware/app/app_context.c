#include "app_context.h"
#include <string.h>

static app_context_t s_ctx;

app_context_t *app_context(void)
{
    return &s_ctx;
}
