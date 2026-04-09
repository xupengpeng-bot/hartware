#ifndef BSP_UART_H
#define BSP_UART_H

#include <stddef.h>
#include <stdint.h>

#include "board_hw_config.h"

int bsp_uart_write(int port, const uint8_t *data, size_t len);
int bsp_uart_read(int port, uint8_t *buf, size_t cap);

void bsp_uart_modem_init(void);

/** 仅 UART4：直接写 BRR 寄存器。 */
void bsp_uart_modem_set_brr(uint32_t brr);

/** UART4：按波特率写 BRR（内部按 RCC 计算内核时钟，与「老曹」工程 PLL 后 USART_Init 一致）。 */
void bsp_uart_modem_set_baud(uint32_t baud);

/** 仅重配 PC10/PC11（4G 用）；上拉 RX，避免与模组侧 CMOS 电平不匹配时一直无 RX。 */
void bsp_uart_modem_reapply_pins(void);

/** 按当前 RCC 重算 UART4 BRR（115200/9600）。 */
void bsp_uart_modem_sync_baud_to_rcc(uint32_t baud);

/** 调试用：打印 RCC_CR/RCC_CFGR/UART4_BRR 及推导的 USART 内核时钟。 */
void bsp_uart_modem_log_rcc(void);

/* CN3 card reader on USART1 (PA9/PA10). */
void bsp_uart_card_reader_init(void);

/* Debug: UART5 PC12/PD2；LAO_CAO 模式为 115200@8MHz 固定 BRR，否则按 PLL 推导。 */
void bsp_uart_debug_init(void);

void bsp_debug_log(const char *s);
void bsp_debug_hex32(const char *tag, uint32_t v);

#endif /* BSP_UART_H */
