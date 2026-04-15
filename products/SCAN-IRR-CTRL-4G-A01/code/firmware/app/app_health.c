#include "app_health.h"
#include "bsp_watchdog.h"

void app_health_init(void)
{
    bsp_watchdog_init();
}

void app_health_poll(void)
{
    bsp_watchdog_feed();
}
