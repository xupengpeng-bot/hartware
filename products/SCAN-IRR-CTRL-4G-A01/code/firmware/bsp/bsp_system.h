#ifndef BSP_SYSTEM_H
#define BSP_SYSTEM_H

#include <stdint.h>

/**
 * 毫秒级阻塞延时 — 由 BSP 实现（空转 / RTOS sleep / SysTick）。
 * 主循环依赖此接口，禁止在量产中留空实现。
 */
void bsp_system_delay_ms(uint32_t ms);

#endif /* BSP_SYSTEM_H */
