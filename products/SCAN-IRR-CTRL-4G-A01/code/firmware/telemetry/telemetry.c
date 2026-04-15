#include "telemetry.h"

#include "proto_heartbeat.h"
#include "proto_register.h"
#include "proto_state_snapshot.h"

int telemetry_build_register(char *buf, size_t cap)
{
    return proto_register_build(buf, cap);
}

int telemetry_build_heartbeat(char *buf, size_t cap)
{
    return proto_heartbeat_build(buf, cap);
}

int telemetry_build_state_snapshot(char *buf, size_t cap)
{
    return proto_state_snapshot_build(buf, cap);
}
