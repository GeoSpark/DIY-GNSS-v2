#ifdef CONFIG_RFM9x
#define MODULE gnss_io
#include "events/gnss_event.h"

#include <caf/events/module_state_event.h>

#include <zephyr/drivers/lora.h>

#include <zephyr/device.h>
//#include <bluetooth/services/nus.h>

static bool io_app_event_handler(const struct app_event_header*);

APP_EVENT_LISTENER(MODULE, io_app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, gnss_event);

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

//static const struct device* uart = DEVICE_DT_GET(DT_CHOSEN(nordic_nus_uart));


#ifdef CONFIG_RFM9x
static const struct device* dev = DEVICE_DT_GET(DT_INST(0, hoperf_rfm9x));

static bool lora_init() {
    if (!device_is_ready(dev)) {
        LOG_ERR("RFM9x not ready.");
        return false;
    }

    struct lora_modem_config cfg;
    cfg.frequency = 868000000;
    cfg.datarate = SF_7;
    cfg.bandwidth = BW_500_KHZ;
    cfg.coding_rate = CR_4_5;
    cfg.preamble_len = 8;
    cfg.tx = false;

    lora_config(dev, &cfg);

    return true;
}
#endif

//static void bt_receive_cb(struct bt_conn* conn, const uint8_t* const data, uint16_t len) {
//
//}
//
//static struct bt_nus_cb nus_cb = {
//        .received = bt_receive_cb,
//};

//static bool init_btuart() {
//    int err = bt_nus_init(&nus_cb);
//
//    if (err != 0) {
//        LOG_ERR("Failed to initialize UART service (err: %d)", err);
//        return false;
//    }
//
//    return true;
//}

static void init() {
#ifdef CONFIG_RFM9x
    if (!lora_init()) {
        module_set_state(MODULE_STATE_ERROR);
        return;
    }
#endif

//    if (!init_btuart()) {
//        module_set_state(MODULE_STATE_ERROR);
//        return;
//    }

    module_set_state(MODULE_STATE_READY);
}

static bool io_app_event_handler(const struct app_event_header* aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event* event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(ble_adv), MODULE_STATE_READY)) {
            init();
        }
    }

//    if (is_gnss_event(aeh)) {
//        bt_nus_send();
//    }

    return false;
}
#endif
