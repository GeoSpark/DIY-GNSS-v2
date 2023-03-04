#ifndef _ROVER_EVENT_H_
#define _ROVER_EVENT_H_

#include <app_event_manager.h>

struct rover_event {
    struct app_event_header header;

    bool active;
};

APP_EVENT_TYPE_DECLARE(rover_event);

#endif
