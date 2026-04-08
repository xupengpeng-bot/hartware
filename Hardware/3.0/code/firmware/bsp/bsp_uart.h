#ifndef BSP_UART_H
#define BSP_UART_H

#include <stddef.h>
#include <stdint.h>

#include "board_hw_config.h"

int bsp_uart_write(int port, const uint8_t *data, size_t len);
int bsp_uart_read(int port, uint8_t *buf, size_t cap);

void bsp_uart_modem_init(void);

/** CN3：初始化 USART1（PA9/PA10），供刷卡 workflow 使用 */
void bsp_uart_card_reader_init(void);

/**
 * 调试：同时初始化
 * - USART2（PA2/PA3，原理图 TX2/RX2），硬件 115200；
 * - CN4：PC12 软 TX（DWT 周期延时）+ PD2 输入占位；
 * 日志每个字节两路各发一次，便于 CN4 或 PA2 任接其一。
 */
void bsp_uart_debug_init(void);

/** 输出一行 ASCII（无换行则自行在字符串里加 \\r\\n） */
void bsp_debug_log(const char *s);

/** 输出 tag=0xXXXXXXXX + \\r\\n */
void bsp_debug_hex32(const char *tag, uint32_t v);

#endif /* BSP_UART_H */
