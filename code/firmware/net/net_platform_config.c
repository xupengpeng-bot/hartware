#include "net_platform_config.h"
#include "fw_build_config.h"
#include "model_config.h"
#include "storage_config.h"

#include <string.h>

static net_platform_endpoint_t s_ep;
static uint8_t                  s_inited;

static void apply_build_defaults(void)
{
    memset(&s_ep, 0, sizeof(s_ep));
    (void)strncpy(s_ep.tcp_host, FW_PLATFORM_TCP_HOST, sizeof(s_ep.tcp_host) - 1U);
    s_ep.tcp_port = (uint16_t)FW_PLATFORM_TCP_PORT;
    (void)strncpy(s_ep.api_path, FW_PLATFORM_API_PATH, sizeof(s_ep.api_path) - 1U);
}

void net_platform_config_init(void)
{
    if (s_inited != 0U) {
        return;
    }
    apply_build_defaults();
    s_inited = 1U;
}

const net_platform_endpoint_t *net_platform_endpoint_get(void)
{
    net_platform_config_init();
    return &s_ep;
}

const char *net_platform_api_path(void)
{
    net_platform_config_init();
    return s_ep.api_path;
}

void net_platform_config_set_tcp(const char *host, uint16_t port)
{
    net_platform_config_init();
    if (host != NULL) {
        (void)strncpy(s_ep.tcp_host, host, sizeof(s_ep.tcp_host) - 1U);
        s_ep.tcp_host[sizeof(s_ep.tcp_host) - 1U] = '\0';
    }
    s_ep.tcp_port = port;
}

void net_platform_config_set_api_path(const char *path)
{
    net_platform_config_init();
    if (path == NULL) {
        s_ep.api_path[0] = '\0';
        return;
    }
    (void)strncpy(s_ep.api_path, path, sizeof(s_ep.api_path) - 1U);
    s_ep.api_path[sizeof(s_ep.api_path) - 1U] = '\0';
}

void net_platform_config_reload_from_device_config(void)
{
    net_platform_config_init();
    device_config_t cfg;
    if (storage_config_load(&cfg) != 0) {
        return;
    }
    if (cfg.platform_tcp_host[0] != '\0') {
        (void)strncpy(s_ep.tcp_host, cfg.platform_tcp_host, sizeof(s_ep.tcp_host) - 1U);
        s_ep.tcp_host[sizeof(s_ep.tcp_host) - 1U] = '\0';
    }
    if (cfg.platform_tcp_port != 0U) {
        s_ep.tcp_port = cfg.platform_tcp_port;
    }
}
