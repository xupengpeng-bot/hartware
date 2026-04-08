#include "bsp_flash.h"

int bsp_flash_read(uint32_t addr, void *buf, size_t len)
{
    (void)addr;
    (void)buf;
    (void)len;
    return -1;
}

int bsp_flash_write(uint32_t addr, const void *buf, size_t len)
{
    (void)addr;
    (void)buf;
    (void)len;
    return -1;
}

int bsp_flash_erase_sector(uint32_t addr)
{
    (void)addr;
    return -1;
}
