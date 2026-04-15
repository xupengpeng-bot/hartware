#ifndef BSP_STATUS_LED_H
#define BSP_STATUS_LED_H

#include <stdint.h>

/**
 * 灯语：默认仅 RELAY1（PC3）= 蜂窝 / TCP（第二路可在 board_hw_config 配置）。
 *
 * L1（PC3）—— 蜂窝 / TCP 链路
 *   - 慢闪 ~1Hz：模组未就绪（common_status.online == false）
 *   - 快闪 ~4Hz：模组在线但 TCP 未连接
 *   - 常亮：TCP 已连接
 *
 * L2（可选第二 GPIO）：注册与心跳（BOARD_HW_PIN_STATUS_LED_B_PORT_C == NONE 时无硬件指示）
 *   - 灭：未注册
 *   - 常亮：已注册
 *   - 已注册时若成功发出 link_ping：短灭 ~180ms
 */
void bsp_status_led_init(void);
void bsp_status_led_poll(uint32_t monotonic_ms);

#endif /* BSP_STATUS_LED_H */
