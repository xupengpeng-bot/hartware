#include "bsp_flash.h"

#include "stm32f1_flash.h"

int bsp_flash_read(uint32_t addr, void *buf, size_t len)
{
    return stm32f1_flash_read(addr, buf, len);
}

int bsp_flash_write(uint32_t addr, const void *buf, size_t len)
{
    return stm32f1_flash_program(addr, buf, len);
}

int bsp_flash_erase_sector(uint32_t addr)
{
    return stm32f1_flash_erase_page(addr);
}
