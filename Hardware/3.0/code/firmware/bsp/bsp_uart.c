#include "bsp_uart.h"

#include <string.h>

#if defined(BOARD_STM32F103)

#define RCC_BASE       0x40021000U
#define GPIOA_BASE     0x40010800U
#define GPIOB_BASE     0x40010C00U
#define AFIO_BASE      0x40010000U
#define GPIOC_BASE     0x40011000U
#define GPIOD_BASE     0x40011400U
#define USART1_BASE    0x40013800U
#define USART2_BASE    0x40004400U
#define UART4_BASE     0x40004C00U

#define RCC_APB2ENR    (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define RCC_APB1ENR    (*((volatile uint32_t *)(RCC_BASE + 0x1CU)))

#define AFIO_MAPR      (*((volatile uint32_t *)(AFIO_BASE + 0x04U)))

#define GPIOA_CRL      (*((volatile uint32_t *)(GPIOA_BASE + 0x00U)))
#define GPIOA_CRH      (*((volatile uint32_t *)(GPIOA_BASE + 0x04U)))
#define GPIOB_CRL      (*((volatile uint32_t *)(GPIOB_BASE + 0x00U)))
#define GPIOC_CRH      (*((volatile uint32_t *)(GPIOC_BASE + 0x04U)))
#define GPIOD_CRL      (*((volatile uint32_t *)(GPIOD_BASE + 0x00U)))
#define GPIOC_BSRR     (*((volatile uint32_t *)(GPIOC_BASE + 0x10U)))

#define USART1_SR      (*((volatile uint32_t *)(USART1_BASE + 0x00U)))
#define USART1_DR      (*((volatile uint32_t *)(USART1_BASE + 0x04U)))
#define USART1_BRR     (*((volatile uint32_t *)(USART1_BASE + 0x08U)))
#define USART1_CR1     (*((volatile uint32_t *)(USART1_BASE + 0x0CU)))

#define USART2_SR      (*((volatile uint32_t *)(USART2_BASE + 0x00U)))
#define USART2_DR      (*((volatile uint32_t *)(USART2_BASE + 0x04U)))
#define USART2_BRR     (*((volatile uint32_t *)(USART2_BASE + 0x08U)))
#define USART2_CR1     (*((volatile uint32_t *)(USART2_BASE + 0x0CU)))

#define UART4_SR       (*((volatile uint32_t *)(UART4_BASE + 0x00U)))
#define UART4_DR       (*((volatile uint32_t *)(UART4_BASE + 0x04U)))
#define UART4_BRR      (*((volatile uint32_t *)(UART4_BASE + 0x08U)))
#define UART4_CR1      (*((volatile uint32_t *)(UART4_BASE + 0x0CU)))

#define RCC_APB2ENR_AFIOEN   (1U << 0)
#define RCC_APB2ENR_IOPAEN   (1U << 2)
#define RCC_APB2ENR_IOPBEN   (1U << 3)
#define RCC_APB2ENR_IOPCEN   (1U << 4)
#define RCC_APB2ENR_IOPDEN   (1U << 5)
#define RCC_APB2ENR_USART1EN (1U << 14)
#define RCC_APB1ENR_USART2EN (1U << 17)
#define RCC_APB1ENR_UART4EN  (1U << 19)

#define AFIO_MAPR_USART2_REMAP (1U << 3)

#define USART_SR_TXE   (1U << 7)
#define USART_SR_RXNE  (1U << 5)
#define USART_CR1_UE   (1U << 13)
#define USART_CR1_TE   (1U << 3)
#define USART_CR1_RE   (1U << 2)

#define USART_115200_8MHZ_BRR 0x45U

#define USART_TX_SPIN_MAX 200000U

static uint8_t s_usart1_cn3_ready;
static uint8_t s_uart4_modem_ready;
static uint8_t s_dbg_ready;

static void usart1_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((USART1_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    USART1_DR = (uint32_t)c;
}

static void usart2_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((USART2_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    USART2_DR = (uint32_t)c;
}

static void uart4_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((UART4_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    UART4_DR = (uint32_t)c;
}

void bsp_uart_card_reader_init(void)
{
    if (s_usart1_cn3_ready != 0U) {
        return;
    }

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    {
        uint32_t crh = GPIOA_CRH;
        crh &= ~(0xFU << 4U);
        crh |= (0xBU << 4U);
        crh &= ~(0xFU << 8U);
        crh |= (0x4U << 8U);
        GPIOA_CRH = crh;
    }

    USART1_CR1 = 0U;
    USART1_BRR = USART_115200_8MHZ_BRR;
    USART1_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    s_usart1_cn3_ready = 1U;
}

void bsp_uart_modem_init(void)
{
    if (s_uart4_modem_ready != 0U) {
        return;
    }

    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;
    RCC_APB1ENR |= RCC_APB1ENR_UART4EN;

    {
        uint32_t crh = GPIOC_CRH;
        crh &= ~(0xFFU << 8U);
        crh |= (0xBU << 8U);
        crh |= (0x4U << 12U);
        GPIOC_CRH = crh;
    }

    UART4_CR1 = 0U;
    UART4_BRR = USART_115200_8MHZ_BRR;
    UART4_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    s_uart4_modem_ready = 1U;
}

static void debug_usart2_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

    /* 确保 USART2 未重映射到 PD5/6，调试口在 PA2/PA3（原理图 TX2/RX2） */
    AFIO_MAPR &= ~AFIO_MAPR_USART2_REMAP;

    {
        uint32_t crl = GPIOA_CRL;
        crl &= ~(0xFFU << 8U);
        crl |= (0xBU << 8U);
        crl |= (0x4U << 12U);
        GPIOA_CRL = crl;
    }

    USART2_CR1 = 0U;
    USART2_BRR = USART_115200_8MHZ_BRR;
    USART2_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/*
 * CN4 PC12 软 TX：不用 DWT（部分环境下 CYCCNT 不递增会导致死等）。
 * 8MHz HSI 下用空转近似 115200 位时间，可按示波器微调 CN4_BIT_DELAY_LOOPS。
 */
#define CN4_BIT_DELAY_LOOPS 80U

static void cn4_soft_delay_bit(void)
{
    volatile uint32_t n = CN4_BIT_DELAY_LOOPS;
    while (n > 0U) {
        n--;
        __asm volatile("nop");
        __asm volatile("nop");
        __asm volatile("nop");
        __asm volatile("nop");
    }
}

static void cn4_gpio_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN;

    {
        uint32_t crh = GPIOC_CRH;
        crh &= ~(0xFU << 16U);
        crh |= (0x3U << 16U);
        GPIOC_CRH = crh;
    }
    GPIOC_BSRR = (1U << 12U);

    {
        uint32_t crl = GPIOD_CRL;
        crl &= ~(0xFU << 8U);
        crl |= (0x4U << 8U);
        GPIOD_CRL = crl;
    }
}

static void cn4_soft_putc(uint8_t c)
{
    uint32_t i;

    GPIOC_BSRR = (1U << (12U + 16U));
    cn4_soft_delay_bit();

    for (i = 0U; i < 8U; i++) {
        if (((uint32_t)c >> i) & 1U) {
            GPIOC_BSRR = (1U << 12U);
        } else {
            GPIOC_BSRR = (1U << (12U + 16U));
        }
        cn4_soft_delay_bit();
    }

    GPIOC_BSRR = (1U << 12U);
    cn4_soft_delay_bit();
}

static void debug_putc_both(uint8_t c)
{
    /* 先硬件 USART2（PA2），再 CN4；任一路阻塞不影响另一路已发数据 */
    usart2_putc(c);
    cn4_soft_putc(c);
}

void bsp_uart_debug_init(void)
{
    if (s_dbg_ready != 0U) {
        return;
    }

    debug_usart2_init();
    cn4_gpio_init();

    s_dbg_ready = 1U;
}

void bsp_debug_log(const char *s)
{
    if (s == NULL) {
        return;
    }
    if (s_dbg_ready == 0U) {
        bsp_uart_debug_init();
    }
    while (*s != '\0') {
        debug_putc_both((uint8_t)*s++);
    }
}

static void put_hex_digit(unsigned x)
{
    static const char *xd = "0123456789abcdef";
    debug_putc_both((uint8_t)xd[x & 0xFU]);
}

void bsp_debug_hex32(const char *tag, uint32_t v)
{
    bsp_debug_log(tag);
    bsp_debug_log("=0x");
    for (int i = 7; i >= 0; i--) {
        put_hex_digit((unsigned)((v >> (unsigned)(i * 4)) & 0xFU));
    }
    bsp_debug_log("\r\n");
}

int bsp_uart_write(int port, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) {
        return 0;
    }
#if defined(BOARD_STM32F103)
    if (port == (int)BOARD_HW_UART_PORT_CARD_READER) {
        if (s_usart1_cn3_ready == 0U) {
            bsp_uart_card_reader_init();
        }
        for (size_t i = 0U; i < len; i++) {
            usart1_putc(data[i]);
        }
        return (int)len;
    }
    if (port == (int)BOARD_HW_UART_PORT_MODEM_4G) {
        if (s_uart4_modem_ready == 0U) {
            bsp_uart_modem_init();
        }
        for (size_t i = 0U; i < len; i++) {
            uart4_putc(data[i]);
        }
        return (int)len;
    }
    if (port == (int)BOARD_HW_UART_PORT_DEBUG) {
        if (s_dbg_ready == 0U) {
            bsp_uart_debug_init();
        }
        for (size_t i = 0U; i < len; i++) {
            debug_putc_both(data[i]);
        }
        return (int)len;
    }
#endif
    (void)port;
    return 0;
}

int bsp_uart_read(int port, uint8_t *buf, size_t cap)
{
#if defined(BOARD_STM32F103)
    if (port == (int)BOARD_HW_UART_PORT_CARD_READER && s_usart1_cn3_ready != 0U && cap > 0U && buf != NULL) {
        if ((USART1_SR & USART_SR_RXNE) != 0U) {
            buf[0] = (uint8_t)(USART1_DR & 0xFFU);
            return 1;
        }
        return 0;
    }
    if (port == (int)BOARD_HW_UART_PORT_MODEM_4G && cap > 0U && buf != NULL) {
        if (s_uart4_modem_ready == 0U) {
            bsp_uart_modem_init();
        }
        if ((UART4_SR & USART_SR_RXNE) != 0U) {
            buf[0] = (uint8_t)(UART4_DR & 0xFFU);
            return 1;
        }
        return 0;
    }
    if (port == (int)BOARD_HW_UART_PORT_DEBUG && s_dbg_ready != 0U && cap > 0U && buf != NULL) {
        if ((USART2_SR & USART_SR_RXNE) != 0U) {
            buf[0] = (uint8_t)(USART2_DR & 0xFFU);
            return 1;
        }
        return 0;
    }
#endif
    (void)port;
    (void)buf;
    (void)cap;
    return 0;
}

#else

void bsp_uart_card_reader_init(void) {}

void bsp_uart_modem_init(void) {}

void bsp_uart_debug_init(void) {}

void bsp_debug_log(const char *s)
{
    (void)s;
}

void bsp_debug_hex32(const char *tag, uint32_t v)
{
    (void)tag;
    (void)v;
}

int bsp_uart_write(int port, const uint8_t *data, size_t len)
{
    (void)port;
    (void)data;
    (void)len;
    return 0;
}

int bsp_uart_read(int port, uint8_t *buf, size_t cap)
{
    (void)port;
    (void)buf;
    (void)cap;
    return 0;
}

#endif /* BOARD_STM32F103 */
