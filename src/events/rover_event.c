#include "events/rover_event.h"

static void log_rover_event(const struct app_event_header* hdr) {
    struct rover_event* event = cast_rover_event(hdr);

    if (event->active) {
        APP_EVENT_MANAGER_LOG(hdr, "Activating rover");
    } else {
        APP_EVENT_MANAGER_LOG(hdr, "Deactivating rover");
    }
}

APP_EVENT_TYPE_DEFINE(rover_event,
        log_rover_event,
        NULL,
        APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));
