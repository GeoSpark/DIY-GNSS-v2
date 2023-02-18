#ifndef _GNSS_EVENT_H_
#define _GNSS_EVENT_H_

#include <app_event_manager.h>

struct gnss_event {
        struct app_event_header header;

        int8_t value1;
        int16_t value2;
        int32_t value3;
        // struct event_dyndata dyndata;
};

APP_EVENT_TYPE_DECLARE(gnss_event);
// APP_EVENT_TYPE_DYNDATA_DECLARE(sample_event);

#endif
