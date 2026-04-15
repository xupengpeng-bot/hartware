#ifndef COMMON_ALARM_H
#define COMMON_ALARM_H

#include <stdint.h>
#include <stddef.h>

#define COMMON_ALARM_MAX 16U

void common_alarm_init(void);
void common_alarm_clear(void);
void common_alarm_raise(uint32_t code);
void common_alarm_clear_code(uint32_t code);
size_t common_alarm_copy(uint32_t *out, size_t max_codes);

#endif /* COMMON_ALARM_H */
