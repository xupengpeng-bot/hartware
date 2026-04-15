/**
 * OTA types and constants — Embedded Controller OTA Spec v1
 * Shared by protocol, storage, and workflow guard.
 */
#ifndef OTA_TYPES_H
#define OTA_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define OTA_VERSION_STRING_MAX   32U
#define OTA_URL_MAX              512U
#define OTA_SHA256_HEX_LEN       65U
#define OTA_TICKET_MAX           64U
#define OTA_ID_MAX               64U
#define OTA_FILE_NAME_MAX        96U
#define OTA_PACKAGE_FORMAT_MAX   32U
#define OTA_ETAG_MAX             96U
#define OTA_STAGE_NAME_MAX       24U
#define OTA_ERROR_MESSAGE_MAX   128U

#define OTA_ERRC_MANIFEST_MISSING            (-20)
#define OTA_ERRC_HTTP_DOWNLOAD               (-21)
#define OTA_ERRC_FLASH_WRITE                 (-22)
#define OTA_ERRC_SIZE_MISMATCH               (-23)
#define OTA_ERRC_SHA_PORT_UNAVAILABLE        (-24)
#define OTA_ERRC_SHA_FINAL                   (-25)
#define OTA_ERRC_SHA256_MISMATCH             (-26)
#define OTA_ERRC_SWITCH_MANIFEST_MISSING     (-27)
#define OTA_ERRC_BOOT_CONTROL_WRITE          (-28)
#define OTA_ERRC_REBOOT_RETURNED             (-29)
#define OTA_ERRC_MANIFEST_SAVE_FAILED        (-33)
#define OTA_ERRC_NOT_READY_TO_DOWNLOAD       (-34)
#define OTA_ERRC_DOWNLOAD_PORT_UNAVAILABLE   (-35)
#define OTA_ERRC_FLASH_ERASE                 (-36)
#define OTA_ERRC_CANCEL_NOT_ALLOWED          (-37)
#define OTA_ERRC_NOT_READY_TO_SWITCH         (-38)
#define OTA_ERRC_REBOOT_PORT_UNAVAILABLE     (-39)
#define OTA_ERRC_HTTP_CONTENT_TYPE_INVALID   (-40)
#define OTA_ERRC_HTTP_ACCEPT_RANGES_INVALID  (-41)
#define OTA_ERRC_HTTP_ETAG_MISMATCH          (-42)
#define OTA_ERRC_HTTP_LENGTH_MISMATCH        (-43)

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_PRECHECKING,
    OTA_STATE_PRECHECK_FAILED,
    OTA_STATE_READY_TO_DOWNLOAD,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_DOWNLOAD_FAILED,
    OTA_STATE_DOWNLOADED,
    OTA_STATE_VERIFYING,
    OTA_STATE_VERIFY_FAILED,
    OTA_STATE_VERIFIED,
    OTA_STATE_WRITING,
    OTA_STATE_WRITE_FAILED,
    OTA_STATE_READY_TO_SWITCH,
    OTA_STATE_SWITCHING,
    OTA_STATE_UPGRADED,
    OTA_STATE_UPGRADE_FAILED,
    OTA_STATE_ROLLING_BACK,
    OTA_STATE_ROLLED_BACK,
} ota_state_t;

typedef enum {
    OTA_LAST_RESULT_NONE = 0,
    OTA_LAST_RESULT_SUCCESS,
    OTA_LAST_RESULT_FAILED,
    OTA_LAST_RESULT_CANCELLED,
} ota_last_result_t;

typedef enum {
    OTA_QUERY_UPGRADE_STATUS = 1,
    OTA_QUERY_UPGRADE_CAPABILITY,
} ota_query_code_t;

typedef enum {
    OTA_ACTION_PREPARE = 1,
    OTA_ACTION_START,
    OTA_ACTION_CANCEL,
    OTA_ACTION_COMMIT,
    OTA_ACTION_ROLLBACK,
} ota_action_code_t;

/** Matches spec EVENT_REPORT event_code strings as numeric ids for embedded use. */
typedef enum {
    OTA_EVENT_OTA_PRECHECK_PASSED = 1,
    OTA_EVENT_OTA_PRECHECK_FAILED,
    OTA_EVENT_OTA_COMMAND_ACKED,
    OTA_EVENT_OTA_DOWNLOAD_PROGRESS,
    OTA_EVENT_OTA_DOWNLOAD_COMPLETED,
    OTA_EVENT_OTA_VERIFY_PASSED,
    OTA_EVENT_OTA_VERIFY_FAILED,
    OTA_EVENT_OTA_WRITE_COMPLETED,
    OTA_EVENT_OTA_SWITCH_SCHEDULED,
    OTA_EVENT_OTA_UPGRADE_SUCCEEDED,
    OTA_EVENT_OTA_UPGRADE_FAILED,
    OTA_EVENT_OTA_ROLLBACK_SUCCEEDED,
    OTA_EVENT_OTA_ROLLBACK_FAILED,
} ota_event_code_t;

typedef struct {
    char     target_version[OTA_VERSION_STRING_MAX];
    char     package_url[OTA_URL_MAX];
    uint32_t package_size;
    char     package_sha256_hex[OTA_SHA256_HEX_LEN];
    char     upgrade_job_id[OTA_ID_MAX];
    char     upgrade_item_id[OTA_ID_MAX];
    char     release_id[OTA_ID_MAX];
    char     release_code[OTA_VERSION_STRING_MAX];
    char     package_artifact_id[OTA_ID_MAX];
    char     package_file_name[OTA_FILE_NAME_MAX];
    char     package_format[OTA_PACKAGE_FORMAT_MAX];
    char     package_etag[OTA_ETAG_MAX];
    uint8_t  min_battery_soc;
    int16_t  min_signal_csq;
    bool     force_upgrade;
    bool     allow_running_upgrade;
    bool     auto_commit;
    char     upgrade_ticket[OTA_TICKET_MAX];
} ota_prepare_payload_t;

typedef struct {
    ota_state_t       ota_state;
    char              ota_stage[OTA_STAGE_NAME_MAX];
    char              target_version[OTA_VERSION_STRING_MAX];
    char              current_version[OTA_VERSION_STRING_MAX];
    char              package_sha256_hex[OTA_SHA256_HEX_LEN];
    char              package_etag[OTA_ETAG_MAX];
    uint8_t           download_progress_pct;
    uint8_t           write_progress_pct;
    ota_last_result_t last_result;
    int32_t           last_error_code;
    char              last_error_message[OTA_ERROR_MESSAGE_MAX];
} ota_upgrade_status_t;

typedef struct {
    bool    ota_supported;
    bool    dual_bank;
    char    package_formats[OTA_PACKAGE_FORMAT_MAX];
    char    compression_formats[OTA_PACKAGE_FORMAT_MAX];
    uint8_t min_battery_soc_default;
    int16_t min_signal_csq_default;
} ota_upgrade_capability_t;

typedef struct {
    int32_t  error_code;
    uint8_t  battery_soc;
    int16_t  signal_csq;
    uint32_t workflow_state;
} ota_event_precheck_failed_t;

typedef struct {
    uint8_t download_progress_pct;
    uint32_t bytes_downloaded;
    uint32_t bytes_total;
} ota_event_download_progress_t;

#endif /* OTA_TYPES_H */
