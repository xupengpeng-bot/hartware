#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include <stdint.h>

void app_scheduler_init(void);
void app_scheduler_tick(uint32_t monotonic_ms);

#endif /* APP_SCHEDULER_H */
