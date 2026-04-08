#include "bsp_voice.h"

void bsp_voice_init(void)
{
}

bool bsp_voice_supported(void)
{
    return false;
}

bool bsp_voice_is_busy(void)
{
    return false;
}

int bsp_voice_play_prompt(const char *prompt_code)
{
    (void)prompt_code;
    return 0;
}
