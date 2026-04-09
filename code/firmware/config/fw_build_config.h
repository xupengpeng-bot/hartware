/**
 * 北向平台默认参数（已写死，一般无需改）。若需覆盖仍可用 -DFW_PLATFORM_TCP_HOST 等或 FW_PLATFORM_CONFIG_DIR。
 */
#ifndef FW_BUILD_CONFIG_H
#define FW_BUILD_CONFIG_H

#ifndef FW_PLATFORM_TCP_HOST
#define FW_PLATFORM_TCP_HOST "xupengpeng.top"
#endif

#ifndef FW_PLATFORM_TCP_PORT
#define FW_PLATFORM_TCP_PORT 32563
#endif

#ifndef FW_PLATFORM_API_PATH
#define FW_PLATFORM_API_PATH "/api/v1"
#endif

#endif /* FW_BUILD_CONFIG_H */
