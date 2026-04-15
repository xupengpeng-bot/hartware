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

int proto_event_report_send_upgrade_report(const char *upgrade_token,
                                           const char *upgrade_job_id,
                                           const char *upgrade_item_id,
                                           const char *release_id,
                                           const char *release_code,
                                           const char *package_artifact_id,
                                           const char *stage,
                                           const char *result,
                                           uint8_t progress_percent,
                                           const char *reason_code,
                                           const char *message,
                                           const char *firmware_version,
                                           const char *checksum);

#endif /* PROTO_EVENT_REPORT_H */
