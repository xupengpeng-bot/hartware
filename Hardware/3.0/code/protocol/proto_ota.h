/**
 * proto_ota — OTA command handling, queries, and EVENT_REPORT emission (Spec §6).
 * JSON tcp-json-v1 framing is done outside; this module uses structured payloads only.
 */
#ifndef PROTO_OTA_H
#define PROTO_OTA_H

#include "ota_port.h"
#include "ota_types.h"
#include <stdbool.h>

typedef struct {
    ota_event_code_t code;
    union {
        ota_event_precheck_failed_t    precheck_failed;
        ota_event_download_progress_t  download_progress;
        int32_t                        error_code;
    } u;
} proto_ota_event_t;

typedef void (*proto_ota_event_cb)(const proto_ota_event_t *event, void *user);

void proto_ota_init(const ota_port_t *port, const char *current_firmware_version,
                    const ota_upgrade_capability_t *capability,
                    proto_ota_event_cb event_cb, void *event_user);

/** Call periodically from main loop to advance download / verify (non-blocking slices). */
void proto_ota_poll(void);

const ota_upgrade_capability_t *proto_ota_get_capability(void);

/** QUERY scope=common query_code=query_upgrade_status */
int proto_ota_query_upgrade_status(ota_upgrade_status_t *out);

/** QUERY scope=common query_code=query_upgrade_capability */
int proto_ota_query_upgrade_capability(ota_upgrade_capability_t *out);

/**
 * EXECUTE_ACTION scope=common — action_code as OTA_ACTION_*.
 * Returns 0 on accept; negative errno-style if rejected (map to COMMAND_NACK).
 */
int proto_ota_execute_action(ota_action_code_t action, const ota_prepare_payload_t *prepare);

void proto_ota_set_current_version(const char *version);

/** Call after successful boot into new firmware (post-REGISTER) to emit ota_upgrade_succeeded. */
void proto_ota_report_upgrade_succeeded_after_boot(void);

#endif /* PROTO_OTA_H */
