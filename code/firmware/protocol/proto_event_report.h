#ifndef PROTO_EVENT_REPORT_H
#define PROTO_EVENT_REPORT_H

#include <stddef.h>

/** Build EVENT_REPORT with arbitrary JSON object body (without outer braces for payload fields). */
int proto_event_report_build(char *buf, size_t cap, const char *session_ref, const char *event_code,
                             const char *payload_fields_json);

#endif /* PROTO_EVENT_REPORT_H */
