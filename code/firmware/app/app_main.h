#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>

void app_main_init(void);
void app_main_loop_iteration(uint32_t monotonic_ms);

#endif /* APP_MAIN_H */
