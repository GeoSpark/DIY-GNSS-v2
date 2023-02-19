#include "events/gnss_event.h"

static void log_gnss_event(const struct app_event_header* hdr) {
    struct gnss_event *event = cast_gnss_event(hdr);

    APP_EVENT_MANAGER_LOG(hdr, "val1=%d val2=%d val3=%d", event->value1,
                    event->value2, event->value3);
}

APP_EVENT_TYPE_DEFINE(gnss_event,
                      log_gnss_event,
                      NULL,
                      APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));
