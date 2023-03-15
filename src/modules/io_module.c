#define MODULE gnss_io

#include "events/gnss_event.h"

#include <caf/events/module_state_event.h>

#ifdef CONFIG_RFM9x
#include <zephyr/drivers/lora.h>
#endif

#include <zephyr/device.h>
#include <bluetooth/services/nus.h>

static bool io_app_event_handler(const struct app_event_header*);

static void bt_connected(struct bt_conn*, uint8_t);

static void bt_disconnected(struct bt_conn*, uint8_t);

#define WORKER_STACK_SIZE 512
#define WORKER_PRIORITY 7
#define NUM_WORK_ITEMS 2

K_THREAD_STACK_DEFINE(io_worker_stack_area, WORKER_STACK_SIZE);

static struct k_work_q io_work_queue;
struct io_work_item_t {
    struct k_work work;
    int8_t* bytes;
    uint16_t size;
};

static struct io_work_item_t io_work_items[NUM_WORK_ITEMS];
static uint8_t cur_work_item = 0;

APP_EVENT_LISTENER(MODULE, io_app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, gnss_event);

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

//static const struct device* uart = DEVICE_DT_GET(DT_CHOSEN(nordic_nus_uart));
static struct bt_conn* cur_conn = NULL;

BT_CONN_CB_DEFINE(MODULE) = {
        .connected = bt_connected,
        .disconnected = bt_disconnected,
};

static void exchange_func(struct bt_conn* conn, uint8_t err, struct bt_gatt_exchange_params* params) {
    ARG_UNUSED(conn);
    ARG_UNUSED(params);

    if (!err) {
        LOG_INF("MTU exchange done");
    } else {
        LOG_WRN("MTU exchange failed (err %" PRIu8 ")", err);
    }
}

static void bt_connected(struct bt_conn* conn, uint8_t con_err) {
    ARG_UNUSED(con_err);
    static struct bt_gatt_exchange_params exchange_params;

    exchange_params.func = exchange_func;
    int err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        LOG_WRN("MTU exchange failed (err %d)", err);
    }

    cur_conn = bt_conn_ref(conn);
}

static void bt_disconnected(struct bt_conn* conn, uint8_t reason) {
    ARG_UNUSED(reason);

    if (conn == cur_conn) {
        bt_conn_unref(cur_conn);
        cur_conn = NULL;
    }
}

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

static bool init_btuart() {
    int err = bt_nus_init(NULL);
//    int err = bt_nus_init(&nus_cb);

    if (err != 0) {
        LOG_ERR("Failed to initialize UART service (err: %d)", err);
        return false;
    }

    return true;
}

static void work_handler(struct k_work* work) {
    struct io_work_item_t* my_work = (struct io_work_item_t*) work;
    bt_nus_send(NULL, my_work->bytes, my_work->size);
}

static bool handle_gnss_event(const struct gnss_event* event) {
    struct io_work_item_t* work_item = &io_work_items[cur_work_item];

    if (!k_work_is_pending(&work_item->work)) {
        work_item->bytes = event->bytes;
        work_item->size = event->size;
        k_work_submit_to_queue(&io_work_queue, &work_item->work);
        cur_work_item = (cur_work_item + 1) % NUM_WORK_ITEMS;
    } else {
        LOG_WRN("Work item still pending");
    }

    return false;
}

static void init() {
#ifdef CONFIG_RFM9x
    if (!lora_init()) {
        module_set_state(MODULE_STATE_ERROR);
        return;
    }
#endif

    if (!init_btuart()) {
        module_set_state(MODULE_STATE_ERROR);
        return;
    }

    k_work_queue_init(&io_work_queue);
    for (uint8_t i = 0; i < NUM_WORK_ITEMS; ++i) {
        k_work_init(&io_work_items[i].work, work_handler);
    }
    k_work_queue_start(&io_work_queue, io_worker_stack_area,
                       K_THREAD_STACK_SIZEOF(io_worker_stack_area), WORKER_PRIORITY,
                       NULL);

    module_set_state(MODULE_STATE_READY);
}

static bool io_app_event_handler(const struct app_event_header* aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event* event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(data_module), MODULE_STATE_READY)) {
            init();
        }
    }

    if (is_gnss_event(aeh)) {
        return handle_gnss_event(cast_gnss_event(aeh));
    }

    return false;
}
