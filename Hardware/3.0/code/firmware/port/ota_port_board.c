#include "ota_port_board.h"
#include "bsp_adc.h"
#include "common_status.h"

#include <stddef.h>

static bool stub_tcp_ok(void *user)
{
    (void)user;
    return true;
}

static int read_battery_soc(uint8_t *out_pct, void *user)
{
    (void)user;
    if (!out_pct) {
        return -1;
    }
    bsp_adc_sample_battery_to_status();
    *out_pct = common_status_get()->battery_soc;
    return 0;
}

static int stub_csq(int16_t *out_csq, void *user)
{
    (void)user;
    if (!out_csq) {
        return -1;
    }
    *out_csq = 18;
    return 0;
}

static int stub_free(uint32_t *out_free, void *user)
{
    (void)user;
    if (!out_free) {
        return -1;
    }
    *out_free = 16U * 1024U * 1024U;
    return 0;
}

static const ota_port_t s_port = {
    .tcp_session_stable  = stub_tcp_ok,
    .get_battery_soc     = read_battery_soc,
    .get_signal_csq      = stub_csq,
    .http_download_chunk = NULL,
    .sha256_init         = NULL,
    .sha256_update       = NULL,
    .sha256_final        = NULL,
    .sha256_free         = NULL,
    .flash_erase_upgrade_region = NULL,
    .flash_write_upgrade_region = NULL,
    .reboot_to_new_image = NULL,
    .storage_free_bytes  = stub_free,
    .user                = NULL,
};

const ota_port_t *ota_port_board(void)
{
    return &s_port;
}
