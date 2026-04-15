#include "bsp_uart.h"

#include <string.h>

#if defined(BOARD_STM32F103)

#include "board_hw_config.h"

#define RCC_BASE       0x40021000U
#define GPIOA_BASE     0x40010800U
#define GPIOC_BASE     0x40011000U
#define GPIOA_ODR      (*((volatile uint32_t *)(GPIOA_BASE + 0x0CU)))
#define GPIOC_ODR      (*((volatile uint32_t *)(GPIOC_BASE + 0x0CU)))
#define GPIOD_BASE     0x40011400U
#define GPIOD_ODR      (*((volatile uint32_t *)(GPIOD_BASE + 0x0CU)))
#define USART1_BASE    0x40013800U
#define USART2_BASE    0x40004400U
#define UART4_BASE     0x40004C00U
#define UART5_BASE     0x40005000U

#define RCC_CR         (*((volatile uint32_t *)(RCC_BASE + 0x00U)))
#define RCC_CFGR       (*((volatile uint32_t *)(RCC_BASE + 0x04U)))
#define RCC_APB2ENR    (*((volatile uint32_t *)(RCC_BASE + 0x18U)))
#define RCC_APB1ENR    (*((volatile uint32_t *)(RCC_BASE + 0x1CU)))
#define NVIC_ISER1     (*((volatile uint32_t *)0xE000E104U))

#define GPIOA_CRL      (*((volatile uint32_t *)(GPIOA_BASE + 0x00U)))
#define GPIOA_CRH      (*((volatile uint32_t *)(GPIOA_BASE + 0x04U)))
#define GPIOC_CRH      (*((volatile uint32_t *)(GPIOC_BASE + 0x04U)))
#define GPIOD_CRL      (*((volatile uint32_t *)(GPIOD_BASE + 0x00U)))
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

#define UART5_SR       (*((volatile uint32_t *)(UART5_BASE + 0x00U)))
#define UART5_DR       (*((volatile uint32_t *)(UART5_BASE + 0x04U)))
#define UART5_BRR      (*((volatile uint32_t *)(UART5_BASE + 0x08U)))
#define UART5_CR1      (*((volatile uint32_t *)(UART5_BASE + 0x0CU)))

#define RCC_APB2ENR_IOPAEN   (1U << 2)
#define RCC_APB2ENR_IOPCEN   (1U << 4)
#define RCC_APB2ENR_IOPDEN   (1U << 5)
#define RCC_APB2ENR_USART1EN (1U << 14)
#define RCC_APB1ENR_USART2EN (1U << 17)
#define RCC_APB1ENR_UART4EN  (1U << 19)
#define RCC_APB1ENR_UART5EN  (1U << 20)

#define USART_SR_TXE   (1U << 7)
#define USART_SR_TC    (1U << 6)
#define USART_SR_RXNE  (1U << 5)
#define USART_SR_ORE   (1U << 3)
#define USART_SR_FE    (1U << 1)
#define USART_SR_NE    (1U << 2)
#define USART_SR_PE    (1U << 0)
#define USART_CR1_UE   (1U << 13)
#define USART_CR1_M    (1U << 12)
#define USART_CR1_PCE  (1U << 10)
#define USART_CR1_PS   (1U << 9)
#define USART_CR1_TE   (1U << 3)
#define USART_CR1_RE   (1U << 2)
#define USART_CR1_RXNEIE (1U << 5)

/* 8MHz 鍐呮牳鏃剁殑鍏稿瀷 BRR锛屼粎浣滃洖閫€锛涙甯哥敤 stm32f103_usart_apb1_kernel_hz()+usart_brr_from_kernel() */
#define USART_115200_8MHZ_BRR 0x45U
#define USART_9600_8MHZ_BRR   0x341U
#define USART_2400_8MHZ_BRR   0xD05U

#define USART_TX_SPIN_MAX 200000U
#define UART1_RX_FIFO_CAP 128U
#define UART2_RX_FIFO_CAP 128U
#define UART4_RX_FIFO_CAP 256U
#define USART1_IRQ_BIT     (1U << (37U - 32U))
#define USART2_IRQ_BIT     (1U << (38U - 32U))
#define UART4_IRQ_BIT      (1U << (52U - 32U))
#define CARD_READER_DEFAULT_BAUD 9600U

#define HSI_VALUE_HZ 8000000U

#if !BOARD_CLOCK_LAO_CAO_HSI_8MHZ

/** 浠呬粠 RCC 瀵勫瓨鍣ㄦ帹瀵?SYSCLK锛圚z锛夛紝涓?SystemCoreClock 鍙橀噺瑙ｈ€︼紝閬垮厤鍙橀噺涓庣‖浠朵笉涓€鑷存椂 UART BRR 閿欒 */
static uint32_t stm32f103_sysclk_hz_from_rcc(void)
{
    uint32_t cfgr = RCC_CFGR;
    uint32_t sw   = (cfgr >> 2) & 3U;

    if (sw == 2U) {
        uint32_t pllmul = (cfgr >> 18) & 0xFU;
        uint32_t mul    = (pllmul <= 13U) ? (pllmul + 2U) : 16U;
        uint32_t pll_in = ((cfgr >> 16) & 1U) ? BOARD_HSE_VALUE_HZ : (HSI_VALUE_HZ / 2U);
        return pll_in * mul;
    }
    if (sw == 1U) {
        return BOARD_HSE_VALUE_HZ;
    }
    return HSI_VALUE_HZ;
}

/** APB1 涓?USART2/3/4/5 鐨勫唴鏍告椂閽熷氨鏄?PCLK1锛?x 瑙勫垯浠呴€傜敤浜庡畾鏃跺櫒銆?*/
static uint32_t stm32f103_usart_apb1_kernel_hz(void)
{
    uint32_t cfgr = RCC_CFGR;
    uint32_t sys  = stm32f103_sysclk_hz_from_rcc();

    {
        uint32_t hpre  = (cfgr >> 4) & 0xFU;
        uint32_t hclk  = sys;
        uint32_t ppre1 = (cfgr >> 8) & 7U;

        if (hpre >= 8U) {
            static const uint8_t ahbtab[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
            hclk = sys >> ahbtab[hpre & 0xFU];
        }
        {
            uint32_t pclk1 = hclk;
            if (ppre1 >= 4U) {
                pclk1 = hclk >> (ppre1 - 3U);
            }
            return pclk1;
        }
    }
}

/** USART1 鍦?APB2锛氭椂閽熶负 PCLK2锛圧M 鏃堕挓鏍戯紝涓?APB1 涓?USART 鐨?2脳PCLK 瑙勫垯涓嶅悓锛?*/
static uint32_t stm32f103_usart_apb2_kernel_hz(void)
{
    uint32_t cfgr = RCC_CFGR;
    uint32_t sys  = stm32f103_sysclk_hz_from_rcc();
    uint32_t hpre = (cfgr >> 4) & 0xFU;
    uint32_t hclk = sys;

    if (hpre >= 8U) {
        static const uint8_t ahbtab[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
        hclk = sys >> ahbtab[hpre & 0xFU];
    }
    {
        uint32_t ppre2 = (cfgr >> 11) & 7U;
        uint32_t pclk2 = hclk;
        if (ppre2 >= 4U) {
            pclk2 = hclk >> (ppre2 - 3U);
        }
        return pclk2;
    }
}

static uint32_t usart_brr_from_kernel_hz(uint32_t ker_hz, uint32_t baud)
{
    uint32_t id;
    uint32_t mantissa;
    uint32_t fraction;

    if (baud == 0U || ker_hz == 0U) {
        return USART_115200_8MHZ_BRR;
    }
    id       = (25U * ker_hz) / (4U * baud);
    mantissa = id / 100U;
    fraction = ((id % 100U) * 16U + 50U) / 100U;
    if (fraction > 15U) {
        mantissa++;
        fraction = 0U;
    }
    return (mantissa << 4) | (fraction & 0xFU);
}

#endif /* !BOARD_CLOCK_LAO_CAO_HSI_8MHZ */

static void uart4_clear_status_errors(void)
{
    uint32_t i;
    for (i = 0U; i < 16U; i++) {
        uint32_t sr = UART4_SR;
        if ((sr & USART_SR_RXNE) != 0U) {
            (void)UART4_DR;
            continue;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            (void)UART4_DR;
            continue;
        }
        break;
    }
}

static void usart1_clear_status_errors(void)
{
    uint32_t i;
    for (i = 0U; i < 16U; i++) {
        uint32_t sr = USART1_SR;
        if ((sr & USART_SR_RXNE) != 0U) {
            (void)USART1_DR;
            continue;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            (void)USART1_DR;
            continue;
        }
        break;
    }
}

static void usart2_clear_status_errors(void)
{
    uint32_t i;
    for (i = 0U; i < 16U; i++) {
        uint32_t sr = USART2_SR;
        if ((sr & USART_SR_RXNE) != 0U) {
            (void)USART2_DR;
            continue;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            (void)USART2_DR;
            continue;
        }
        break;
    }
}

static uint8_t s_usart1_cn3_ready;
static uint8_t s_usart2_rs485_ready;
static uint8_t s_uart4_modem_ready;
static uint8_t s_dbg_ready;
static volatile uint8_t  s_uart1_rx_fifo[UART1_RX_FIFO_CAP];
static volatile uint16_t s_uart1_rx_head;
static volatile uint16_t s_uart1_rx_tail;
static volatile uint8_t  s_uart2_rx_fifo[UART2_RX_FIFO_CAP];
static volatile uint16_t s_uart2_rx_head;
static volatile uint16_t s_uart2_rx_tail;
static volatile uint8_t  s_uart4_rx_fifo[UART4_RX_FIFO_CAP];
static volatile uint16_t s_uart4_rx_head;
static volatile uint16_t s_uart4_rx_tail;
static volatile uint32_t s_uart4_rx_fifo_drop_count;
static volatile uint32_t s_uart4_rx_ore_count;
static volatile uint32_t s_uart4_rx_fe_count;
static volatile uint32_t s_uart4_rx_ne_count;
static volatile uint32_t s_uart4_rx_pe_count;

static uint32_t irq_save_disable(void)
{
    uint32_t primask;

    __asm volatile(
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r"(primask)
        :
        : "memory");
    return primask;
}

static void irq_restore(uint32_t primask)
{
    __asm volatile(
        "msr primask, %0\n"
        :
        : "r"(primask)
        : "memory");
}

static void uart1_fifo_reset(void)
{
    s_uart1_rx_head = 0U;
    s_uart1_rx_tail = 0U;
}

static void uart1_fifo_push(uint8_t c)
{
    uint16_t next = (uint16_t)((s_uart1_rx_tail + 1U) % UART1_RX_FIFO_CAP);
    if (next == s_uart1_rx_head) {
        s_uart1_rx_head = (uint16_t)((s_uart1_rx_head + 1U) % UART1_RX_FIFO_CAP);
    }
    s_uart1_rx_fifo[s_uart1_rx_tail] = c;
    s_uart1_rx_tail = next;
}

static int uart1_fifo_pop(uint8_t *out)
{
    if (out == NULL || s_uart1_rx_head == s_uart1_rx_tail) {
        return 0;
    }
    *out = s_uart1_rx_fifo[s_uart1_rx_head];
    s_uart1_rx_head = (uint16_t)((s_uart1_rx_head + 1U) % UART1_RX_FIFO_CAP);
    return 1;
}

static void uart2_fifo_reset(void)
{
    s_uart2_rx_head = 0U;
    s_uart2_rx_tail = 0U;
}

static void uart2_fifo_push(uint8_t c)
{
    uint16_t next = (uint16_t)((s_uart2_rx_tail + 1U) % UART2_RX_FIFO_CAP);
    if (next == s_uart2_rx_head) {
        s_uart2_rx_head = (uint16_t)((s_uart2_rx_head + 1U) % UART2_RX_FIFO_CAP);
    }
    s_uart2_rx_fifo[s_uart2_rx_tail] = c;
    s_uart2_rx_tail = next;
}

static int uart2_fifo_pop(uint8_t *out)
{
    if (out == NULL || s_uart2_rx_head == s_uart2_rx_tail) {
        return 0;
    }
    *out = s_uart2_rx_fifo[s_uart2_rx_head];
    s_uart2_rx_head = (uint16_t)((s_uart2_rx_head + 1U) % UART2_RX_FIFO_CAP);
    return 1;
}

static void uart4_fifo_reset(void)
{
    s_uart4_rx_head = 0U;
    s_uart4_rx_tail = 0U;
}

static void uart4_fifo_push(uint8_t c)
{
    uint16_t next = (uint16_t)((s_uart4_rx_tail + 1U) % UART4_RX_FIFO_CAP);
    if (next == s_uart4_rx_head) {
        s_uart4_rx_head = (uint16_t)((s_uart4_rx_head + 1U) % UART4_RX_FIFO_CAP);
        s_uart4_rx_fifo_drop_count++;
    }
    s_uart4_rx_fifo[s_uart4_rx_tail] = c;
    s_uart4_rx_tail = next;
}

static int uart4_fifo_pop(uint8_t *out)
{
    if (out == NULL || s_uart4_rx_head == s_uart4_rx_tail) {
        return 0;
    }
    *out = s_uart4_rx_fifo[s_uart4_rx_head];
    s_uart4_rx_head = (uint16_t)((s_uart4_rx_head + 1U) % UART4_RX_FIFO_CAP);
    return 1;
}

static void usart1_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((USART1_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    USART1_DR = (uint32_t)c;
}

static void uart4_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((UART4_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    UART4_DR = (uint32_t)c;
}

static void usart2_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((USART2_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    USART2_DR = (uint32_t)c;
}

static void uart5_putc(uint8_t c)
{
    uint32_t guard = USART_TX_SPIN_MAX;
    while ((UART5_SR & USART_SR_TXE) == 0U && guard > 0U) {
        guard--;
    }
    UART5_DR = (uint32_t)c;
}

static void rs485_dir_set(uint8_t tx_enable)
{
    if (tx_enable != 0U) {
        GPIOA_ODR |= (1U << BOARD_HW_PIN_RS485_DIR_PORT_A);
    } else {
        GPIOA_ODR &= ~(1U << BOARD_HW_PIN_RS485_DIR_PORT_A);
    }
}

static void rs485_turnaround_delay(void)
{
    volatile uint32_t guard;

    for (guard = 0U; guard < 256U; guard++) {
        __asm volatile ("nop");
    }
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
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    USART1_BRR = USART_9600_8MHZ_BRR;
#else
    USART1_BRR = usart_brr_from_kernel_hz(stm32f103_usart_apb2_kernel_hz(), CARD_READER_DEFAULT_BAUD);
#endif
    uart1_fifo_reset();
    USART1_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_ISER1 = USART1_IRQ_BIT;
    usart1_clear_status_errors();

    s_usart1_cn3_ready = 1U;
}

void bsp_uart_rs485_init(uint32_t baud)
{
    uint32_t target = baud;

    if (target == 0U) {
        target = 9600U;
    }

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

    {
        uint32_t crl = GPIOA_CRL;

        crl &= ~(0xFU << 4U);
        crl |= (0x3U << 4U);
        crl &= ~(0xFU << 8U);
        crl |= (0xBU << 8U);
        crl &= ~(0xFU << 12U);
        crl |= (0x8U << 12U);
        GPIOA_CRL = crl;
    }
    GPIOA_ODR |= (1U << BOARD_HW_PIN_USART2_RX_PORT_A);

    rs485_dir_set(0U);
    USART2_CR1 = 0U;
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    if (target == 2400U) {
        USART2_BRR = USART_2400_8MHZ_BRR;
    } else if (target == 9600U) {
        USART2_BRR = USART_9600_8MHZ_BRR;
    } else {
        USART2_BRR = USART_115200_8MHZ_BRR;
    }
#else
    USART2_BRR = usart_brr_from_kernel_hz(stm32f103_usart_apb1_kernel_hz(), target);
#endif
    uart2_fifo_reset();
    USART2_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE |
                 USART_CR1_M | USART_CR1_PCE | USART_CR1_UE;
    NVIC_ISER1 = USART2_IRQ_BIT;
    usart2_clear_status_errors();

    s_usart2_rs485_ready = 1U;
}

void bsp_uart_modem_reapply_pins(void)
{
#if defined(BOARD_STM32F103)
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;
    {
        uint32_t crh = GPIOC_CRH;
        crh &= ~(0xFFU << 8U);
        /* Match the legacy firmware: PC10=TX4 AF_PP, PC11=RX4 floating input. */
        crh |= (0xBU << 8U);
        crh |= (0x4U << 12U);
        GPIOC_CRH = crh;
    }
#endif
}

void bsp_uart_modem_init(void)
{
    if (s_uart4_modem_ready != 0U) {
        return;
    }

    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;
    RCC_APB1ENR |= RCC_APB1ENR_UART4EN;

    bsp_uart_modem_reapply_pins();

    UART4_CR1 = 0U;
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    UART4_BRR = USART_115200_8MHZ_BRR;
#else
    UART4_BRR = usart_brr_from_kernel_hz(stm32f103_usart_apb1_kernel_hz(), 115200U);
#endif
    uart4_fifo_reset();
    UART4_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_ISER1 = UART4_IRQ_BIT;

    uart4_clear_status_errors();

    s_uart4_modem_ready = 1U;
}

void bsp_uart_modem_set_baud(uint32_t baud)
{
#if defined(BOARD_STM32F103)
    uint32_t target = baud;

    if (s_uart4_modem_ready == 0U) {
        bsp_uart_modem_init();
    }
    if (target != 9600U && target != 115200U) {
        target = 115200U;
    }
    UART4_CR1 &= ~USART_CR1_UE;
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    UART4_BRR = (target == 9600U) ? USART_9600_8MHZ_BRR : USART_115200_8MHZ_BRR;
#else
    {
        uint32_t ker = stm32f103_usart_apb1_kernel_hz();
        UART4_BRR = usart_brr_from_kernel_hz(ker, target);
    }
#endif
    uart4_fifo_reset();
    UART4_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    uart4_clear_status_errors();
#else
    (void)baud;
#endif
}

void bsp_uart_modem_sync_baud_to_rcc(uint32_t baud)
{
    bsp_uart_modem_set_baud(baud);
}

void bsp_uart_modem_set_brr(uint32_t brr)
{
#if defined(BOARD_STM32F103)
    if (s_uart4_modem_ready == 0U) {
        bsp_uart_modem_init();
    }
    UART4_CR1 &= ~USART_CR1_UE;
    UART4_BRR = brr;
    uart4_fifo_reset();
    UART4_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
#else
    (void)brr;
#endif
}

void bsp_uart_modem_log_rcc(void)
{
#if defined(BOARD_STM32F103)
# if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    bsp_debug_log("[4G] clock: LAO_CAO HSI 8MHz, UART BRR table\r\n");
    bsp_debug_hex32("[4G] RCC_CFGR  ", RCC_CFGR);
    bsp_debug_hex32("[4G] UART4_BRR ", UART4_BRR);
# else
    {
        uint32_t ker = stm32f103_usart_apb1_kernel_hz();
        bsp_debug_hex32("[4G] RCC_CR    ", RCC_CR);
        bsp_debug_hex32("[4G] RCC_CFGR  ", RCC_CFGR);
        bsp_debug_hex32("[4G] USART_ker ", ker);
        bsp_debug_hex32("[4G] UART4_BRR ", UART4_BRR);
    }
# endif
#else
#endif
}

static void cn4_uart5_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN;
    RCC_APB1ENR |= RCC_APB1ENR_UART5EN;

    {
        uint32_t crh = GPIOC_CRH;
        crh &= ~(0xFU << 16U);
        crh |= (0xBU << 16U);
        GPIOC_CRH = crh;
    }

    {
        uint32_t crl = GPIOD_CRL;
        crl &= ~(0xFU << 8U);
        crl |= (0x8U << 8U);
        GPIOD_CRL = crl;
    }
    GPIOD_ODR |= (1U << 2U);

    UART5_CR1 = 0U;
#if BOARD_CLOCK_LAO_CAO_HSI_8MHZ
    if (BOARD_UART_DEBUG_BAUD == 9600U) {
        UART5_BRR = USART_9600_8MHZ_BRR;
    } else {
        UART5_BRR = USART_115200_8MHZ_BRR;
    }
#else
    UART5_BRR = usart_brr_from_kernel_hz(stm32f103_usart_apb1_kernel_hz(), BOARD_UART_DEBUG_BAUD);
#endif
    UART5_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void debug_putc_both(uint8_t c)
{
    uart5_putc(c);
}

void bsp_uart_debug_init(void)
{
    if (s_dbg_ready != 0U) {
        return;
    }

    cn4_uart5_init();

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
    if (port == (int)BOARD_HW_UART_PORT_RS485) {
        if (s_usart2_rs485_ready == 0U) {
            bsp_uart_rs485_init(2400U);
        }
        rs485_dir_set(1U);
        rs485_turnaround_delay();
        for (size_t i = 0U; i < len; i++) {
            usart2_putc(data[i]);
        }
        {
            uint32_t guard = USART_TX_SPIN_MAX;
            while ((USART2_SR & USART_SR_TC) == 0U && guard > 0U) {
                guard--;
            }
        }
        rs485_dir_set(0U);
        return (int)len;
    }
    if (port == (int)BOARD_HW_UART_PORT_MODEM_4G) {
        if (s_uart4_modem_ready == 0U) {
            bsp_uart_modem_init();
        }
        for (size_t i = 0U; i < len; i++) {
            uart4_putc(data[i]);
        }
        {
            uint32_t guard = USART_TX_SPIN_MAX;
            while ((UART4_SR & USART_SR_TC) == 0U && guard > 0U) {
                guard--;
            }
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
        size_t n = 0U;
        uint32_t sr;
        while (n < cap && uart1_fifo_pop(&buf[n]) != 0) {
            n++;
        }
        while (n < cap) {
            sr = USART1_SR;
            if ((sr & USART_SR_RXNE) != 0U) {
                buf[n++] = (uint8_t)(USART1_DR & 0xFFU);
                continue;
            }
            if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
                (void)USART1_DR;
                continue;
            }
            break;
        }
        return (int)n;
    }
    if (port == (int)BOARD_HW_UART_PORT_MODEM_4G && cap > 0U && buf != NULL) {
        uint32_t sr;
        uint8_t  b;
        uint32_t primask;
        if (s_uart4_modem_ready == 0U) {
            bsp_uart_modem_init();
        }
        primask = irq_save_disable();
        if (uart4_fifo_pop(&b) != 0) {
            irq_restore(primask);
            buf[0] = b;
            return 1;
        }
        sr = UART4_SR;
        /* UART4 RX is IRQ-driven, but a byte can still be sitting in DR before
         * the IRQ handler has pushed it into the software FIFO. Read FIFO/DR
         * inside one atomic section so the same modem byte cannot be consumed by
         * both the IRQ handler and this polling path. */
        if ((sr & USART_SR_RXNE) != 0U) {
            buf[0] = (uint8_t)(UART4_DR & 0xFFU);
            irq_restore(primask);
            return 1;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            (void)UART4_DR;
        }
        irq_restore(primask);
        return 0;
    }
    if (port == (int)BOARD_HW_UART_PORT_RS485 && cap > 0U && buf != NULL) {
        size_t n = 0U;
        uint32_t sr;

        if (s_usart2_rs485_ready == 0U) {
            bsp_uart_rs485_init(2400U);
        }
        while (n < cap && uart2_fifo_pop(&buf[n]) != 0) {
            n++;
        }
        while (n < cap) {
            sr = USART2_SR;
            if ((sr & USART_SR_RXNE) != 0U) {
                buf[n++] = (uint8_t)(USART2_DR & 0xFFU);
                continue;
            }
            if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
                (void)USART2_DR;
                continue;
            }
            break;
        }
        return (int)n;
    }
    if (port == (int)BOARD_HW_UART_PORT_DEBUG && s_dbg_ready != 0U && cap > 0U && buf != NULL) {
        if ((UART5_SR & USART_SR_RXNE) != 0U) {
            buf[0] = (uint8_t)(UART5_DR & 0xFFU);
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

void USART2_IRQHandler(void)
{
    uint32_t sr;

    for (;;) {
        sr = USART2_SR;
        if ((sr & USART_SR_RXNE) != 0U) {
            uart2_fifo_push((uint8_t)(USART2_DR & 0xFFU));
            continue;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            (void)USART2_DR;
            continue;
        }
        break;
    }
}

void UART4_IRQHandler(void)
{
    uint32_t sr;

    for (;;) {
        sr = UART4_SR;
        if ((sr & USART_SR_RXNE) != 0U) {
            uart4_fifo_push((uint8_t)(UART4_DR & 0xFFU));
            continue;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            if ((sr & USART_SR_ORE) != 0U) {
                s_uart4_rx_ore_count++;
            }
            if ((sr & USART_SR_FE) != 0U) {
                s_uart4_rx_fe_count++;
            }
            if ((sr & USART_SR_NE) != 0U) {
                s_uart4_rx_ne_count++;
            }
            if ((sr & USART_SR_PE) != 0U) {
                s_uart4_rx_pe_count++;
            }
            (void)UART4_DR;
            continue;
        }
        break;
    }
}

void bsp_uart_modem_take_rx_diag(uint32_t *fifo_drop_count,
                                 uint32_t *ore_count,
                                 uint32_t *fe_count,
                                 uint32_t *ne_count,
                                 uint32_t *pe_count)
{
#if defined(BOARD_STM32F103)
    if (fifo_drop_count != NULL) {
        *fifo_drop_count = s_uart4_rx_fifo_drop_count;
    }
    if (ore_count != NULL) {
        *ore_count = s_uart4_rx_ore_count;
    }
    if (fe_count != NULL) {
        *fe_count = s_uart4_rx_fe_count;
    }
    if (ne_count != NULL) {
        *ne_count = s_uart4_rx_ne_count;
    }
    if (pe_count != NULL) {
        *pe_count = s_uart4_rx_pe_count;
    }
    s_uart4_rx_fifo_drop_count = 0U;
    s_uart4_rx_ore_count = 0U;
    s_uart4_rx_fe_count = 0U;
    s_uart4_rx_ne_count = 0U;
    s_uart4_rx_pe_count = 0U;
#else
    if (fifo_drop_count != NULL) {
        *fifo_drop_count = 0U;
    }
    if (ore_count != NULL) {
        *ore_count = 0U;
    }
    if (fe_count != NULL) {
        *fe_count = 0U;
    }
    if (ne_count != NULL) {
        *ne_count = 0U;
    }
    if (pe_count != NULL) {
        *pe_count = 0U;
    }
#endif
}

void USART1_IRQHandler(void)
{
    uint32_t sr;

    for (;;) {
        sr = USART1_SR;
        if ((sr & USART_SR_RXNE) != 0U) {
            uart1_fifo_push((uint8_t)(USART1_DR & 0xFFU));
            continue;
        }
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            (void)USART1_DR;
            continue;
        }
        break;
    }
}

#else

void bsp_uart_card_reader_init(void) {}

void bsp_uart_modem_init(void) {}

void bsp_uart_rs485_init(uint32_t baud)
{
    (void)baud;
}

void bsp_uart_modem_set_brr(uint32_t brr)
{
    (void)brr;
}

void bsp_uart_modem_reapply_pins(void) {}

void bsp_uart_modem_set_baud(uint32_t baud)
{
    (void)baud;
}

void bsp_uart_modem_sync_baud_to_rcc(uint32_t baud)
{
    (void)baud;
}

void bsp_uart_modem_log_rcc(void) {}

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

#endif
