/**
 * 北向平台默认参数（TCP）。
 * 当前目标：xupengpeng.top:32563
 * 覆盖方式：CMake -DFW_PLATFORM_TCP_HOST=... / -DFW_PLATFORM_TCP_PORT=...，或 FW_PLATFORM_CONFIG_DIR 指向自定义头。
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

#ifndef FW_PLATFORM_APN
#define FW_PLATFORM_APN "CMIOT"
#endif

#endif /* FW_BUILD_CONFIG_H */
