/**
 * Application singletons — wire concrete BSP / net instances here.
 */
#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <stdint.h>

typedef struct {
    uint32_t monotonic_ms;
} app_context_t;

app_context_t *app_context(void);

#endif /* APP_CONTEXT_H */
