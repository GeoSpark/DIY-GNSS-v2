#ifndef _GNSS_EVENT_H_
#define _GNSS_EVENT_H_

#include <app_event_manager.h>

struct gnss_event {
        struct app_event_header header;

        int8_t* bytes;
        uint32_t size;
};

APP_EVENT_TYPE_DECLARE(gnss_event);

#endif
