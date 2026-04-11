#ifndef PROTO_EVENT_REPORT_H
#define PROTO_EVENT_REPORT_H

#include <stdint.h>

int proto_event_report_send_min(const char *session_ref,
                                const char *event_code,
                                const char *reject_code,
                                const char *message,
                                const char *target_ref);

int proto_event_report_send_counter_reset(const char *session_ref,
                                          const char *reason_code,
                                          uint32_t meter_epoch,
                                          uint32_t runtime_sec,
                                          float total_m3,
                                          float energy_kwh);

#endif /* PROTO_EVENT_REPORT_H */
