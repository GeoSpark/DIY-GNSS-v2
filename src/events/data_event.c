#include "events/data_event.h"

static void log_data_event(const struct app_event_header* aeh) {
    APP_EVENT_MANAGER_LOG(aeh, "Data event");
}

APP_EVENT_TYPE_DEFINE(data_event,
        log_data_event,
        NULL,
        APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));
