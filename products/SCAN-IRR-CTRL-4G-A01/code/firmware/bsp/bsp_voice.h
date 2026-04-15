#ifndef BSP_VOICE_H
#define BSP_VOICE_H

#include <stdbool.h>

void bsp_voice_init(void);
bool bsp_voice_supported(void);
bool bsp_voice_is_busy(void);
int  bsp_voice_play_prompt(const char *prompt_code);

#endif /* BSP_VOICE_H */
