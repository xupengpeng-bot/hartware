#ifndef STM32F1_FLASH_H
#define STM32F1_FLASH_H

#include <stddef.h>
#include <stdint.h>

int stm32f1_flash_read(uint32_t addr, void *buf, size_t len);
int stm32f1_flash_erase_page(uint32_t page_addr);
int stm32f1_flash_program(uint32_t addr, const void *buf, size_t len);

#endif /* STM32F1_FLASH_H */
