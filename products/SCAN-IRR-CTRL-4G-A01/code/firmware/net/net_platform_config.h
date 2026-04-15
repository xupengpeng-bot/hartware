#ifndef NET_PLATFORM_CONFIG_H
#define NET_PLATFORM_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define NET_PLATFORM_HOST_MAX  96U
#define NET_PLATFORM_PATH_MAX 160U

typedef struct {
    char     tcp_host[NET_PLATFORM_HOST_MAX];
    uint16_t tcp_port;
    char     api_path[NET_PLATFORM_PATH_MAX];
} net_platform_endpoint_t;

void net_platform_config_init(void);

const net_platform_endpoint_t *net_platform_endpoint_get(void);

/** 返回供 OTA / HTTP 拼接使用的路径前缀（与 FW_PLATFORM_API_PATH 一致，可被运行时更新）。 */
const char *net_platform_api_path(void);

/** 运行时覆盖（例如从 Flash 配置区读取后调用）。host 可为 NULL 表示不修改该项。 */
void net_platform_config_set_tcp(const char *host, uint16_t port);
void net_platform_config_set_api_path(const char *path);

/** 按 device_config 中的 platform_tcp_* 合并到编译期默认值（需已 storage_config_load 有效或 seed 后）。 */
void net_platform_config_reload_from_device_config(void);

#endif /* NET_PLATFORM_CONFIG_H */
