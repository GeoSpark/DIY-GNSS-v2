#define MODULE main

#include "px1122r.h"

#include <app_event_manager.h>
#include <caf/events/module_state_event.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

void main(void) {
    LOG_INF("********");
    LOG_INF("DIY GNSS");
    LOG_INF("********");

    if (app_event_manager_init() != 0) {
		LOG_ERR("Application Event Manager not initialized");
		return;
    }

	module_set_state(MODULE_STATE_READY);
}
