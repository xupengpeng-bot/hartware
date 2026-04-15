#include "proto_ota.h"
#include "storage_upgrade.h"
#include "workflow_upgrade_guard.h"

#include <string.h>

#define OTA_CHUNK_BYTES        256U
#define OTA_ERR_DENIED         (-1)
#define OTA_ERR_BAD_STATE      (-2)
#define OTA_ERR_PORT           (-3)
#define OTA_ERR_ROLLBACK_PHASE2 (-4)
#define OTA_ERR_DUPLICATE      (-5)

static const ota_port_t          *s_port;
static proto_ota_event_cb         s_event_cb;
static void                      *s_event_user;
static ota_upgrade_capability_t   s_cap;
static char                       s_current_version[OTA_VERSION_STRING_MAX];
static char                       s_last_upgrade_ticket[OTA_TICKET_MAX];

static ota_state_t    s_state;
static ota_last_result_t s_last_result;
static int32_t        s_last_error_code;
static char           s_last_error_message[OTA_ERROR_MESSAGE_MAX];

static uint32_t       s_download_offset;
static uint8_t        s_dl_progress_reported;
static void          *s_sha_ctx;
static uint8_t        s_chunk[OTA_CHUNK_BYTES];

static void persist_progress(void);
static const char *ota_switch_error_message(int32_t err);
static const char *ota_download_error_message(int32_t err);

static bool ota_state_accepts_duplicate_ack(ota_state_t state)
{
    switch (state) {
    case OTA_STATE_DOWNLOADING:
    case OTA_STATE_DOWNLOADED:
    case OTA_STATE_VERIFYING:
    case OTA_STATE_VERIFIED:
    case OTA_STATE_WRITING:
    case OTA_STATE_READY_TO_SWITCH:
    case OTA_STATE_SWITCHING:
        return true;
    default:
        return false;
    }
}

static bool ota_state_is_retryable_failure(ota_state_t state)
{
    switch (state) {
    case OTA_STATE_PRECHECK_FAILED:
    case OTA_STATE_DOWNLOAD_FAILED:
    case OTA_STATE_VERIFY_FAILED:
    case OTA_STATE_WRITE_FAILED:
    case OTA_STATE_UPGRADE_FAILED:
        return true;
    default:
        return false;
    }
}

static void ota_reset_attempt_state(int clear_manifest, int clear_ticket)
{
    if (s_sha_ctx && s_port && s_port->sha256_free) {
        s_port->sha256_free(s_sha_ctx, s_port->user);
    }
    s_sha_ctx = NULL;
    s_download_offset = 0U;
    s_dl_progress_reported = 0U;
    if (clear_manifest != 0) {
        storage_upgrade_clear_manifest();
    }
    if (clear_ticket != 0) {
        s_last_upgrade_ticket[0] = '\0';
    }
    s_state = OTA_STATE_IDLE;
    persist_progress();
}

static void ota_copy_cstr(char *dst, size_t cap, const char *src)
{
    size_t i = 0U;

    if (dst == NULL || cap == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (i + 1U < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void persist_progress(void)
{
    uint8_t dlp = 0;
    uint8_t wrp = 0;
    if (s_state == OTA_STATE_DOWNLOADING || s_state == OTA_STATE_DOWNLOADED) {
        ota_prepare_payload_t m;
        if (storage_upgrade_load_manifest(&m) == 0 && m.package_size > 0U) {
            dlp = (uint8_t)((s_download_offset * 100U) / m.package_size);
            if (dlp > 100U) {
                dlp = 100U;
            }
        }
        wrp = dlp;
    }
    (void)storage_upgrade_save_ota_state(s_state, dlp, wrp);
}

static void set_error(int32_t code, const char *msg)
{
    s_last_error_code = code;
    if (msg) {
        ota_copy_cstr(s_last_error_message, sizeof(s_last_error_message), msg);
    } else {
        s_last_error_message[0] = '\0';
    }
}

static void emit_event(const proto_ota_event_t *ev)
{
    if (s_event_cb) {
        s_event_cb(ev, s_event_user);
    }
}

static int bin_to_hex_lower(const uint8_t *in, size_t n, char *out, size_t out_len)
{
    static const char xd[] = "0123456789abcdef";
    if (out_len < n * 2U + 1U) {
        return -1;
    }
    for (size_t i = 0; i < n; i++) {
        out[i * 2U]     = xd[in[i] >> 4];
        out[i * 2U + 1U] = xd[in[i] & 15];
    }
    out[n * 2U] = '\0';
    return 0;
}

static size_t ota_strnlen_local(const char *s, size_t max_len)
{
    size_t i;
    for (i = 0U; i < max_len && s[i] != '\0'; i++) {
    }
    return i;
}

static int hex64_equals_digest(const char *hex, const uint8_t digest[32])
{
    char comp[OTA_SHA256_HEX_LEN];
    if (bin_to_hex_lower(digest, 32U, comp, sizeof(comp)) != 0) {
        return 0;
    }
    size_t n = ota_strnlen_local(hex, OTA_SHA256_HEX_LEN);
    if (n < 64U) {
        return 0;
    }
    for (size_t i = 0; i < 64U; i++) {
        char a = hex[i];
        char b = comp[i];
        if (a >= 'A' && a <= 'F') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'F') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static void emit_upgrade_failed(int32_t err, const char *msg)
{
    set_error(err, msg);
    s_last_result = OTA_LAST_RESULT_FAILED;
    proto_ota_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.code = OTA_EVENT_OTA_UPGRADE_FAILED;
    ev.u.error_code = err;
    emit_event(&ev);
}

static const char *ota_switch_error_message(int32_t err)
{
    switch (err) {
    case OTA_ERRC_SWITCH_MANIFEST_MISSING:
        return "switch manifest missing";
    case OTA_ERRC_BOOT_CONTROL_WRITE:
        return "boot control write failed";
    case OTA_ERRC_REBOOT_RETURNED:
        return "reboot returned";
    default:
        return "switch failed";
    }
}

const char *proto_ota_stage_name(ota_state_t state, ota_last_result_t last_result)
{
    switch (state) {
    case OTA_STATE_PRECHECKING:
    case OTA_STATE_READY_TO_DOWNLOAD:
        return "accepted";
    case OTA_STATE_DOWNLOADING:
        return "downloading";
    case OTA_STATE_DOWNLOADED:
    case OTA_STATE_VERIFYING:
        return "downloaded";
    case OTA_STATE_VERIFIED:
        return "verified";
    case OTA_STATE_WRITING:
    case OTA_STATE_READY_TO_SWITCH:
        return "staged";
    case OTA_STATE_SWITCHING:
        return "rebooting";
    case OTA_STATE_UPGRADED:
        return "boot_confirmed";
    case OTA_STATE_PRECHECK_FAILED:
    case OTA_STATE_DOWNLOAD_FAILED:
    case OTA_STATE_VERIFY_FAILED:
    case OTA_STATE_WRITE_FAILED:
    case OTA_STATE_UPGRADE_FAILED:
    case OTA_STATE_ROLLING_BACK:
    case OTA_STATE_ROLLED_BACK:
        return "failed";
    case OTA_STATE_IDLE:
    default:
        if (last_result == OTA_LAST_RESULT_SUCCESS) {
            return "boot_confirmed";
        }
        if (last_result == OTA_LAST_RESULT_FAILED ||
            last_result == OTA_LAST_RESULT_CANCELLED) {
            return "failed";
        }
        return "idle";
    }
}

static const char *ota_download_error_message(int32_t err)
{
    switch (err) {
    case OTA_ERRC_HTTP_CONTENT_TYPE_INVALID:
        return "content-type invalid";
    case OTA_ERRC_HTTP_ACCEPT_RANGES_INVALID:
        return "accept-ranges invalid";
    case OTA_ERRC_HTTP_ETAG_MISMATCH:
        return "etag mismatch";
    case OTA_ERRC_HTTP_LENGTH_MISMATCH:
        return "content-length mismatch";
    default:
        return "http";
    }
}

static int precheck(const ota_prepare_payload_t *p)
{
    workflow_upgrade_deny_reason_t deny;
    uint8_t tcp_stable = 0U;
    if (!workflow_upgrade_guard_allow_upgrade(p->allow_running_upgrade, &deny)) {
        set_error((int32_t)deny, "workflow busy");
        proto_ota_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.code = OTA_EVENT_OTA_PRECHECK_FAILED;
        ev.u.precheck_failed.error_code = (int32_t)deny;
        if (s_port && s_port->get_battery_soc) {
            (void)s_port->get_battery_soc(&ev.u.precheck_failed.battery_soc, s_port->user);
        }
        if (s_port && s_port->get_signal_csq) {
            (void)s_port->get_signal_csq(&ev.u.precheck_failed.signal_csq, s_port->user);
        }
        ev.u.precheck_failed.workflow_state = workflow_upgrade_guard_workflow_state_for_event();
        emit_event(&ev);
        return -1;
    }
    if (s_port && s_port->tcp_session_stable) {
        if (!s_port->tcp_session_stable(s_port->user)) {
            set_error(-10, "tcp not stable");
            proto_ota_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.code = OTA_EVENT_OTA_PRECHECK_FAILED;
            ev.u.precheck_failed.error_code = -10;
            emit_event(&ev);
            return -1;
        }
        tcp_stable = 1U;
    }
    if (s_port && s_port->get_battery_soc) {
        uint8_t soc = 0;
        if (s_port->get_battery_soc(&soc, s_port->user) == 0) {
            if (!p->force_upgrade && soc < p->min_battery_soc) {
                set_error(-11, "battery low");
                proto_ota_event_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.code = OTA_EVENT_OTA_PRECHECK_FAILED;
                ev.u.precheck_failed.error_code = -11;
                ev.u.precheck_failed.battery_soc = soc;
                emit_event(&ev);
                return -1;
            }
        }
    }
    if (s_port && s_port->get_signal_csq) {
        int16_t csq = 0;
        if (s_port->get_signal_csq(&csq, s_port->user) == 0) {
            int16_t min_signal_csq = p->min_signal_csq;
            if (!p->force_upgrade && tcp_stable != 0U && min_signal_csq > 0) {
                min_signal_csq -= 1;
            }
            if (!p->force_upgrade && csq < min_signal_csq) {
                set_error(-12, "signal low");
                proto_ota_event_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.code = OTA_EVENT_OTA_PRECHECK_FAILED;
                ev.u.precheck_failed.error_code = -12;
                ev.u.precheck_failed.signal_csq = csq;
                emit_event(&ev);
                return -1;
            }
        }
    }
    if (s_port && s_port->storage_free_bytes && p->package_size > 0U) {
        uint32_t free_b = 0;
        if (s_port->storage_free_bytes(&free_b, s_port->user) == 0) {
            if (free_b < p->package_size) {
                set_error(-13, "storage low");
                proto_ota_event_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.code = OTA_EVENT_OTA_PRECHECK_FAILED;
                ev.u.precheck_failed.error_code = -13;
                emit_event(&ev);
                return -1;
            }
        }
    }
    return 0;
}

void proto_ota_init(const ota_port_t *port, const char *current_firmware_version,
                    const ota_upgrade_capability_t *capability,
                    proto_ota_event_cb event_cb, void *event_user)
{
    s_port = port;
    s_event_cb = event_cb;
    s_event_user = event_user;
    memset(&s_cap, 0, sizeof(s_cap));
    s_cap.ota_supported = true;
    s_cap.dual_bank = false;
    if (capability) {
        s_cap = *capability;
    }
    ota_copy_cstr(s_current_version, sizeof(s_current_version), current_firmware_version);

    s_state = OTA_STATE_IDLE;
    s_last_result = OTA_LAST_RESULT_NONE;
    s_last_error_code = 0;
    s_last_error_message[0] = '\0';
    s_download_offset = 0U;
    s_dl_progress_reported = 0U;
    s_sha_ctx = NULL;
    s_last_upgrade_ticket[0] = '\0';

    (void)storage_upgrade_init();
}

void proto_ota_set_current_version(const char *version)
{
    ota_copy_cstr(s_current_version, sizeof(s_current_version), version);
}

void proto_ota_report_upgrade_succeeded_after_boot(void)
{
    s_state = OTA_STATE_UPGRADED;
    s_last_result = OTA_LAST_RESULT_SUCCESS;
    set_error(0, NULL);
    proto_ota_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.code = OTA_EVENT_OTA_UPGRADE_SUCCEEDED;
    emit_event(&ev);
    storage_upgrade_clear_manifest();
    s_state = OTA_STATE_IDLE;
    persist_progress();
}

const ota_upgrade_capability_t *proto_ota_get_capability(void)
{
    return &s_cap;
}

void proto_ota_poll(void)
{
    if (!s_port || s_state != OTA_STATE_DOWNLOADING) {
        return;
    }
    ota_prepare_payload_t m;
    if (storage_upgrade_load_manifest(&m) != 0) {
        s_state = OTA_STATE_DOWNLOAD_FAILED;
        emit_upgrade_failed(OTA_ERRC_MANIFEST_MISSING, "manifest missing");
        persist_progress();
        return;
    }
    if (!s_port->http_download_chunk || !s_port->flash_write_upgrade_region) {
        s_state = OTA_STATE_DOWNLOAD_FAILED;
        emit_upgrade_failed(OTA_ERR_PORT, "port");
        persist_progress();
        return;
    }
    if (s_download_offset == 0U && s_sha_ctx == NULL) {
        if (s_port->flash_erase_upgrade_region &&
            s_port->flash_erase_upgrade_region(s_port->user) != 0) {
            s_state = OTA_STATE_WRITE_FAILED;
            emit_upgrade_failed(OTA_ERRC_FLASH_ERASE, "flash erase");
            persist_progress();
            return;
        }
        if (s_port->sha256_init) {
            (void)s_port->sha256_init(&s_sha_ctx, s_port->user);
        }
    }
    size_t nread = 0;
    int hr = s_port->http_download_chunk(&m, s_download_offset, s_chunk,
                                         sizeof(s_chunk), &nread, s_port->user);
    if (hr != 0) {
        s_state = OTA_STATE_DOWNLOAD_FAILED;
        emit_upgrade_failed(hr, ota_download_error_message(hr));
        persist_progress();
        return;
    }
    if (nread > 0U) {
        if (s_port->sha256_update && s_sha_ctx) {
            (void)s_port->sha256_update(s_sha_ctx, s_chunk, nread, s_port->user);
        }
        if (s_port->flash_write_upgrade_region(s_download_offset, s_chunk, nread, s_port->user) != 0) {
            s_state = OTA_STATE_WRITE_FAILED;
            emit_upgrade_failed(OTA_ERRC_FLASH_WRITE, "flash write");
            persist_progress();
            return;
        }
        s_download_offset += (uint32_t)nread;
        uint8_t pct = 0U;
        if (m.package_size > 0U) {
            pct = (uint8_t)((s_download_offset * 100U) / m.package_size);
            if (pct > 100U) {
                pct = 100U;
            }
        }
        if (pct >= (uint8_t)(s_dl_progress_reported + 10U) || pct == 100U) {
            proto_ota_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.code = OTA_EVENT_OTA_DOWNLOAD_PROGRESS;
            ev.u.download_progress.download_progress_pct = pct;
            ev.u.download_progress.bytes_downloaded = s_download_offset;
            ev.u.download_progress.bytes_total = m.package_size;
            emit_event(&ev);
            s_dl_progress_reported = (pct / 10U) * 10U;
        }
        persist_progress();
    }
    if (nread == 0U) {
        if (s_download_offset != m.package_size) {
            s_state = OTA_STATE_DOWNLOAD_FAILED;
            emit_upgrade_failed(OTA_ERRC_SIZE_MISMATCH, "size mismatch");
            persist_progress();
            return;
        }
        s_state = OTA_STATE_DOWNLOADED;
        persist_progress();
        s_state = OTA_STATE_VERIFYING;
        persist_progress();
        uint8_t digest[32];
        if (!s_port->sha256_init || !s_port->sha256_final || !s_port->sha256_free || !s_sha_ctx) {
            s_state = OTA_STATE_VERIFY_FAILED;
            emit_upgrade_failed(OTA_ERRC_SHA_PORT_UNAVAILABLE, "sha256 port");
            persist_progress();
            return;
        }
        if (s_port->sha256_final(s_sha_ctx, digest, s_port->user) != 0) {
            s_port->sha256_free(s_sha_ctx, s_port->user);
            s_sha_ctx = NULL;
            s_state = OTA_STATE_VERIFY_FAILED;
            emit_upgrade_failed(OTA_ERRC_SHA_FINAL, "sha256 final");
            persist_progress();
            return;
        }
        s_port->sha256_free(s_sha_ctx, s_port->user);
        s_sha_ctx = NULL;
        if (!hex64_equals_digest(m.package_sha256_hex, digest)) {
            s_state = OTA_STATE_VERIFY_FAILED;
            proto_ota_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.code = OTA_EVENT_OTA_VERIFY_FAILED;
            ev.u.error_code = OTA_ERRC_SHA256_MISMATCH;
            emit_event(&ev);
            set_error(OTA_ERRC_SHA256_MISMATCH, "hash mismatch");
            s_last_result = OTA_LAST_RESULT_FAILED;
            proto_ota_event_t ev_fail;
            memset(&ev_fail, 0, sizeof(ev_fail));
            ev_fail.code = OTA_EVENT_OTA_UPGRADE_FAILED;
            ev_fail.u.error_code = OTA_ERRC_SHA256_MISMATCH;
            emit_event(&ev_fail);
            persist_progress();
            return;
        }
        proto_ota_event_t ev_ok;
        s_state = OTA_STATE_DOWNLOADED;
        persist_progress();
        memset(&ev_ok, 0, sizeof(ev_ok));
        ev_ok.code = OTA_EVENT_OTA_DOWNLOAD_COMPLETED;
        emit_event(&ev_ok);
        s_state = OTA_STATE_VERIFIED;
        persist_progress();
        memset(&ev_ok, 0, sizeof(ev_ok));
        ev_ok.code = OTA_EVENT_OTA_VERIFY_PASSED;
        emit_event(&ev_ok);
        s_state = OTA_STATE_READY_TO_SWITCH;
        persist_progress();
        memset(&ev_ok, 0, sizeof(ev_ok));
        ev_ok.code = OTA_EVENT_OTA_WRITE_COMPLETED;
        emit_event(&ev_ok);
        proto_ota_event_t ev_sw;
        memset(&ev_sw, 0, sizeof(ev_sw));
        ev_sw.code = OTA_EVENT_OTA_SWITCH_SCHEDULED;
        emit_event(&ev_sw);
        if (m.auto_commit) {
            (void)proto_ota_execute_action(OTA_ACTION_COMMIT, NULL);
        }
    }
}

int proto_ota_query_upgrade_status(ota_upgrade_status_t *out)
{
    ota_prepare_payload_t m;
    ota_state_t persisted_state;
    uint8_t persisted_dl = 0U;
    uint8_t persisted_wr = 0U;

    if (!out) {
        return -1;
    }
    memset(&m, 0, sizeof(m));
    memset(out, 0, sizeof(*out));
    out->ota_state = s_state;
    ota_copy_cstr(out->ota_stage, sizeof(out->ota_stage),
                  proto_ota_stage_name(s_state, s_last_result));
    ota_copy_cstr(out->current_version, sizeof(out->current_version), s_current_version);
    out->last_result = s_last_result;
    out->last_error_code = s_last_error_code;
    ota_copy_cstr(out->last_error_message, sizeof(out->last_error_message), s_last_error_message);
    if (storage_upgrade_load_ota_state(&persisted_state, &persisted_dl, &persisted_wr) == 0) {
        (void)persisted_state;
        out->download_progress_pct = persisted_dl;
        out->write_progress_pct = persisted_wr;
    }
    if (storage_upgrade_load_manifest(&m) == 0) {
        ota_copy_cstr(out->target_version, sizeof(out->target_version), m.target_version);
        ota_copy_cstr(out->package_sha256_hex, sizeof(out->package_sha256_hex), m.package_sha256_hex);
        ota_copy_cstr(out->package_etag, sizeof(out->package_etag), m.package_etag);
    } else if (s_last_result == OTA_LAST_RESULT_SUCCESS) {
        ota_copy_cstr(out->target_version, sizeof(out->target_version), s_current_version);
    }
    if (s_state == OTA_STATE_DOWNLOADING && m.package_size > 0U) {
        out->download_progress_pct =
            (uint8_t)((s_download_offset * 100U) / m.package_size);
        if (out->download_progress_pct > 100U) {
            out->download_progress_pct = 100U;
        }
        out->write_progress_pct = out->download_progress_pct;
    } else if (s_state == OTA_STATE_DOWNLOADED ||
               s_state == OTA_STATE_VERIFYING ||
               s_state == OTA_STATE_VERIFIED ||
               s_state == OTA_STATE_WRITING ||
               s_state == OTA_STATE_READY_TO_SWITCH ||
               s_state == OTA_STATE_SWITCHING ||
               s_last_result == OTA_LAST_RESULT_SUCCESS) {
        out->download_progress_pct = 100U;
        out->write_progress_pct = 100U;
    }
    return 0;
}

int proto_ota_query_upgrade_capability(ota_upgrade_capability_t *out)
{
    if (!out) {
        return -1;
    }
    *out = s_cap;
    return 0;
}

int proto_ota_execute_action(ota_action_code_t action, const ota_prepare_payload_t *prepare)
{
    if (!s_port) {
        return OTA_ERR_PORT;
    }
    switch (action) {
    case OTA_ACTION_PREPARE:
        if (!prepare) {
            return OTA_ERR_DENIED;
        }
        if (prepare->upgrade_ticket[0] == '\0') {
            set_error(-30, "missing upgrade token");
            return OTA_ERR_DENIED;
        }
        if (ota_state_is_retryable_failure(s_state)) {
            ota_reset_attempt_state(1, 1);
        }
        if (strcmp(s_last_upgrade_ticket, prepare->upgrade_ticket) == 0) {
            set_error(-31, "duplicate upgrade token");
            return OTA_ERR_DUPLICATE;
        }
        if (s_state != OTA_STATE_IDLE) {
            set_error(-32, "upgrade busy");
            return OTA_ERR_BAD_STATE;
        }
        s_state = OTA_STATE_PRECHECKING;
        persist_progress();
        if (precheck(prepare) != 0) {
            s_state = OTA_STATE_PRECHECK_FAILED;
            s_last_result = OTA_LAST_RESULT_FAILED;
            ota_copy_cstr(s_last_upgrade_ticket, sizeof(s_last_upgrade_ticket), prepare->upgrade_ticket);
            persist_progress();
            return OTA_ERR_DENIED;
        }
        if (storage_upgrade_save_manifest(prepare) != 0) {
            s_state = OTA_STATE_PRECHECK_FAILED;
            set_error(OTA_ERRC_MANIFEST_SAVE_FAILED, "manifest save failed");
            return OTA_ERR_DENIED;
        }
        {
            proto_ota_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.code = OTA_EVENT_OTA_PRECHECK_PASSED;
            emit_event(&ev);
        }
        s_last_result = OTA_LAST_RESULT_NONE;
        set_error(0, NULL);
        ota_copy_cstr(s_last_upgrade_ticket, sizeof(s_last_upgrade_ticket), prepare->upgrade_ticket);
        s_state = OTA_STATE_READY_TO_DOWNLOAD;
        persist_progress();
        return 0;

    case OTA_ACTION_START:
        if (s_state != OTA_STATE_READY_TO_DOWNLOAD) {
            set_error(OTA_ERRC_NOT_READY_TO_DOWNLOAD, "not ready to download");
            return OTA_ERR_BAD_STATE;
        }
        if (!s_port->http_download_chunk || !s_port->flash_write_upgrade_region) {
            set_error(OTA_ERRC_DOWNLOAD_PORT_UNAVAILABLE, "download port unavailable");
            return OTA_ERR_PORT;
        }
        s_download_offset = 0U;
        s_dl_progress_reported = 0U;
        s_sha_ctx = NULL;
        s_state = OTA_STATE_DOWNLOADING;
        {
            proto_ota_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.code = OTA_EVENT_OTA_COMMAND_ACKED;
            emit_event(&ev);
        }
        persist_progress();
        return 0;

    case OTA_ACTION_CANCEL:
        if (s_state == OTA_STATE_IDLE) {
            return 0;
        }
        if (s_state == OTA_STATE_SWITCHING || s_state == OTA_STATE_UPGRADED) {
            set_error(OTA_ERRC_CANCEL_NOT_ALLOWED, "cannot cancel now");
            return OTA_ERR_BAD_STATE;
        }
        ota_reset_attempt_state(1, 1);
        s_last_result = OTA_LAST_RESULT_CANCELLED;
        persist_progress();
        return 0;

    case OTA_ACTION_COMMIT:
        if (s_state != OTA_STATE_READY_TO_SWITCH) {
            set_error(OTA_ERRC_NOT_READY_TO_SWITCH, "not ready to switch");
            return OTA_ERR_BAD_STATE;
        }
        if (!s_port->reboot_to_new_image) {
            set_error(OTA_ERRC_REBOOT_PORT_UNAVAILABLE, "reboot port unavailable");
            return OTA_ERR_PORT;
        }
        s_state = OTA_STATE_SWITCHING;
        persist_progress();
        {
            int reboot_rc = s_port->reboot_to_new_image(s_port->user);
            if (reboot_rc == 0) {
                reboot_rc = OTA_ERRC_REBOOT_RETURNED;
            }
            s_state = OTA_STATE_UPGRADE_FAILED;
            emit_upgrade_failed(reboot_rc, ota_switch_error_message(reboot_rc));
            persist_progress();
        }
        return OTA_ERR_PORT;

    case OTA_ACTION_ROLLBACK:
        (void)prepare;
        return OTA_ERR_ROLLBACK_PHASE2;

    default:
        return OTA_ERR_DENIED;
    }
}

bool proto_ota_is_prepare_duplicate_accepted(const ota_prepare_payload_t *prepare, ota_state_t *state_out)
{
    if (state_out != NULL) {
        *state_out = s_state;
    }
    if (prepare == NULL || prepare->upgrade_ticket[0] == '\0') {
        return false;
    }
    if (strcmp(s_last_upgrade_ticket, prepare->upgrade_ticket) != 0) {
        return false;
    }
    return ota_state_accepts_duplicate_ack(s_state);
}
