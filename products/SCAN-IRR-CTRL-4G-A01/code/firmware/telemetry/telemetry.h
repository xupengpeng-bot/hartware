#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stddef.h>

int telemetry_build_register(char *buf, size_t cap);
int telemetry_build_heartbeat(char *buf, size_t cap);
int telemetry_build_state_snapshot(char *buf, size_t cap);

#endif /* TELEMETRY_H */
