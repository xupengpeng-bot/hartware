#include <stdint.h>

/* 覆盖 startup 中弱符号 Default_Handler，便于调试器在 HardFault 停住（两灯常亮且无串口时区分 fault 与死循环） */
void HardFault_Handler(void)
{
    volatile uint32_t n = 0U;
    for (;;) {
        n++;
    }
}
