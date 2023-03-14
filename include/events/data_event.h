#ifndef _DATA_EVENT_H_
#define _DATA_EVENT_H_

#include "config.h"

#include <app_event_manager.h>

enum data_event_type {
    DATA_EVENT_CONFIG_INITIAL,
    DATA_EVENT_CONFIG_UPDATE
};

struct data_event {
    struct app_event_header header;

    enum data_event_type event_type;
    struct config_t config;
};

APP_EVENT_TYPE_DECLARE(data_event);

#endif
