#define MODULE lora

#include <caf/events/module_state_event.h>
#include <zephyr/device.h>

static bool lora_app_event_handler(const struct app_event_header*);

APP_EVENT_LISTENER(MODULE, lora_app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

static const struct device* dev = DEVICE_DT_GET(DT_INST(0, hoperf_rfm9x));

static void lora_init() {
    if (!device_is_ready(dev)) {
        LOG_ERR("RFM9x not ready.");
        module_set_state(MODULE_STATE_ERROR);
        return;
    }

    module_set_state(MODULE_STATE_READY);
}


static bool lora_app_event_handler(const struct app_event_header* aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event* event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
            lora_init();
        }
    }

    return false;
}
