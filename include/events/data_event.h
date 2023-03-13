#ifndef _DATA_EVENT_H_
#define _DATA_EVENT_H_

#include "config.h"

#include <app_event_manager.h>

struct data_event {
    struct app_event_header header;

    struct config_t config;
};

APP_EVENT_TYPE_DECLARE(data_event);

#endif
