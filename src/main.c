#include "px1122r.h"
#include "gnss_event.h"

#include <app_event_manager.h>
#include <zephyr/logging/log.h>

#define MODULE main
#include <caf/events/module_state_event.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

void main(void) {
	if (app_event_manager_init() != 0) {
		LOG_ERR("Application Event Manager not initialized");
		return;
	}

	module_set_state(MODULE_STATE_READY);
}
