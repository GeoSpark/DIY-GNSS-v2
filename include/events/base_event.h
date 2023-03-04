#ifndef _BASE_EVENT_H_
#define _BASE_EVENT_H_

#include <app_event_manager.h>

struct base_event {
    struct app_event_header header;

    bool active;
};

APP_EVENT_TYPE_DECLARE(base_event);

#endif
