#include "events/base_event.h"

static void log_base_event(const struct app_event_header* hdr) {
    struct base_event* event = cast_base_event(hdr);

    if (event->active) {
        APP_EVENT_MANAGER_LOG(hdr, "Activating base");
    } else {
        APP_EVENT_MANAGER_LOG(hdr, "Deactivating base");
    }
}

APP_EVENT_TYPE_DEFINE(base_event,
        log_base_event,
        NULL,
        APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));
