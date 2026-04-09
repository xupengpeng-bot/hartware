#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include <stddef.h>
#include <stdint.h>

int bsp_flash_read(uint32_t addr, void *buf, size_t len);
int bsp_flash_write(uint32_t addr, const void *buf, size_t len);
int bsp_flash_erase_sector(uint32_t addr);

#endif /* BSP_FLASH_H */
