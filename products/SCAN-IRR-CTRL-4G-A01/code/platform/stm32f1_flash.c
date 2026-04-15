#include "stm32f1_flash.h"

#include "flash_layout.h"

#include <string.h>

#define STM32_FLASH_REG_BASE   0x40022000u

#define STM32_FLASH_KEYR       (*((volatile uint32_t *)(STM32_FLASH_REG_BASE + 0x04u)))
#define STM32_FLASH_SR         (*((volatile uint32_t *)(STM32_FLASH_REG_BASE + 0x0cu)))
#define STM32_FLASH_CR         (*((volatile uint32_t *)(STM32_FLASH_REG_BASE + 0x10u)))
#define STM32_FLASH_AR         (*((volatile uint32_t *)(STM32_FLASH_REG_BASE + 0x14u)))

#define STM32_FLASH_KEY1       0x45670123u
#define STM32_FLASH_KEY2       0xCDEF89ABu

#define STM32_FLASH_SR_BSY     (1u << 0)
#define STM32_FLASH_SR_PGERR   (1u << 2)
#define STM32_FLASH_SR_WRPRT   (1u << 4)
#define STM32_FLASH_SR_EOP     (1u << 5)

#define STM32_FLASH_CR_PG      (1u << 0)
#define STM32_FLASH_CR_PER     (1u << 1)
#define STM32_FLASH_CR_STRT    (1u << 6)
#define STM32_FLASH_CR_LOCK    (1u << 7)

static int flash_range_is_valid(uint32_t addr, size_t len)
{
    uint32_t end;

    if (len == 0u) {
        return 1;
    }
    if (addr < STM32F103_FLASH_BASE) {
        return 0;
    }
    if ((uint64_t)addr + (uint64_t)len > (uint64_t)STM32F103_FLASH_END) {
        return 0;
    }
    end = addr + (uint32_t)len;
    if (end < addr) {
        return 0;
    }
    return 1;
}

static int flash_wait_ready(void)
{
    uint32_t spin = 0u;

    while ((STM32_FLASH_SR & STM32_FLASH_SR_BSY) != 0u) {
        spin++;
        if (spin > 4000000u) {
            return -1;
        }
    }
    if ((STM32_FLASH_SR & (STM32_FLASH_SR_PGERR | STM32_FLASH_SR_WRPRT)) != 0u) {
        STM32_FLASH_SR = STM32_FLASH_SR_PGERR | STM32_FLASH_SR_WRPRT;
        return -1;
    }
    if ((STM32_FLASH_SR & STM32_FLASH_SR_EOP) != 0u) {
        STM32_FLASH_SR = STM32_FLASH_SR_EOP;
    }
    return 0;
}

static int flash_unlock(void)
{
    if ((STM32_FLASH_CR & STM32_FLASH_CR_LOCK) == 0u) {
        return 0;
    }
    STM32_FLASH_KEYR = STM32_FLASH_KEY1;
    STM32_FLASH_KEYR = STM32_FLASH_KEY2;
    return (STM32_FLASH_CR & STM32_FLASH_CR_LOCK) == 0u ? 0 : -1;
}

static void flash_lock(void)
{
    STM32_FLASH_CR |= STM32_FLASH_CR_LOCK;
}

int stm32f1_flash_read(uint32_t addr, void *buf, size_t len)
{
    if (buf == NULL || !flash_range_is_valid(addr, len)) {
        return -1;
    }
    if (len == 0u) {
        return 0;
    }
    memcpy(buf, (const void *)addr, len);
    return 0;
}

int stm32f1_flash_erase_page(uint32_t page_addr)
{
    int rc;

    if ((page_addr % STM32F103_FLASH_PAGE_SIZE_BYTES) != 0u) {
        return -1;
    }
    if (!flash_range_is_valid(page_addr, STM32F103_FLASH_PAGE_SIZE_BYTES)) {
        return -1;
    }
    if (flash_unlock() != 0) {
        return -1;
    }
    if (flash_wait_ready() != 0) {
        flash_lock();
        return -1;
    }

    STM32_FLASH_CR |= STM32_FLASH_CR_PER;
    STM32_FLASH_AR = page_addr;
    STM32_FLASH_CR |= STM32_FLASH_CR_STRT;
    rc = flash_wait_ready();
    STM32_FLASH_CR &= ~STM32_FLASH_CR_PER;
    flash_lock();
    return rc;
}

int stm32f1_flash_program(uint32_t addr, const void *buf, size_t len)
{
    const uint8_t *src = (const uint8_t *)buf;
    size_t i;

    if (len == 0u) {
        return 0;
    }
    if (src == NULL || (addr & 1u) != 0u || !flash_range_is_valid(addr, len)) {
        return -1;
    }
    if (flash_unlock() != 0) {
        return -1;
    }
    if (flash_wait_ready() != 0) {
        flash_lock();
        return -1;
    }

    for (i = 0u; i < len; i += 2u) {
        uint16_t half = src[i];
        if (i + 1u < len) {
            half |= (uint16_t)src[i + 1u] << 8;
        } else {
            half |= 0xFF00u;
        }

        STM32_FLASH_CR |= STM32_FLASH_CR_PG;
        *(volatile uint16_t *)(addr + (uint32_t)i) = half;
        if (flash_wait_ready() != 0) {
            STM32_FLASH_CR &= ~STM32_FLASH_CR_PG;
            flash_lock();
            return -1;
        }
        STM32_FLASH_CR &= ~STM32_FLASH_CR_PG;
        if (*(volatile uint16_t *)(addr + (uint32_t)i) != half) {
            flash_lock();
            return -1;
        }
    }

    flash_lock();
    return 0;
}
