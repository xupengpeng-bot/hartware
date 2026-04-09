#ifndef BSP_UART_H
#define BSP_UART_H

#include <stddef.h>
#include <stdint.h>

#include "board_hw_config.h"

int bsp_uart_write(int port, const uint8_t *data, size_t len);
int bsp_uart_read(int port, uint8_t *buf, size_t cap);

void bsp_uart_modem_init(void);

/* CN3 card reader on USART1 (PA9/PA10). */
void bsp_uart_card_reader_init(void);

/* Debug output: UART5 PC12(TX5)/PD2(RX5), 115200 8N1 @ 8MHz HSI. */
void bsp_uart_debug_init(void);

void bsp_debug_log(const char *s);
void bsp_debug_hex32(const char *tag, uint32_t v);

#endif /* BSP_UART_H */
