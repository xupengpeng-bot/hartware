#ifndef PROTO_HEARTBEAT_H
#define PROTO_HEARTBEAT_H

#include <stddef.h>
#include <stdint.h>

/**
 * 轻量保活：~100B 级，用于长连接存活与平台判在线（hb_kind=ping）。
 * seq 单调递增便于丢包与乱序检测。
 */
int proto_heartbeat_build_ping(char *buf, size_t cap, uint32_t seq, uint32_t uptime_sec);

/**
 * 完整体征：4G/电池/就绪等（hb_kind=vitals），低频或变化触发。
 */
int proto_heartbeat_build_vitals(char *buf, size_t cap);

/** 等价于 proto_heartbeat_build_vitals，兼容旧调用。 */
int proto_heartbeat_build(char *buf, size_t cap);

#endif /* PROTO_HEARTBEAT_H */
