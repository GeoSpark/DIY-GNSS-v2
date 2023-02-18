#include "px1122r.h"
// #include "gnss_event.h"

#define MODULE gnss
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);
static const struct device* dev = DEVICE_DT_GET(DT_INST(0, skytraq_px1122r));

static bool handle_button_event(const struct button_event* event) {
    if (event->pressed) {
        LOG_DBG("Here!");
        px1122r_send_data(dev);
    }

    return false;
}

static void init_fn(void) {
	if (!device_is_ready(dev)) {
		LOG_ERR("PX1122R not ready.");
		module_set_state(MODULE_STATE_ERROR);
		return;
	}

	module_set_state(MODULE_STATE_READY);
}

static bool app_event_handler(const struct app_event_header *aeh) {
	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			init_fn();

			return false;
		}

		return false;
	}
    
    if (is_button_event(aeh)) {
        return handle_button_event(cast_button_event(aeh));
    }

    return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
// APP_EVENT_SUBSCRIBE(MODULE, gnss_event);
APP_EVENT_SUBSCRIBE(MODULE, button_event);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
