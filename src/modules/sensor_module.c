#ifdef CONFIG_MAX17048
#define MODULE battery

#include <caf/events/module_state_event.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/adv_prov.h>

static bool battery_app_event_handler(const struct app_event_header*);

static void battery_timer_update(struct k_timer*);

static void battery_timer_work_handler(struct k_work*);

static ssize_t battery_bt_read_level(struct bt_conn*, const struct bt_gatt_attr*, void*, uint16_t, uint16_t);

static void battery_bt_ccc_cfg_changed(const struct bt_gatt_attr*, uint16_t);

static void battery_bt_connected(struct bt_conn*, uint8_t);

static void battery_bt_disconnected(struct bt_conn*, uint8_t);

K_TIMER_DEFINE(battery_timer, battery_timer_update, NULL);
K_WORK_DEFINE(battery_timer_work, battery_timer_work_handler);


APP_EVENT_LISTENER(MODULE, battery_app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);

BT_GATT_SERVICE_DEFINE(battery_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ, battery_bt_read_level, NULL,
                                              NULL),
                       BT_GATT_CCC(battery_bt_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

BT_CONN_CB_DEFINE(conn_callbacks) = {
        .connected = battery_bt_connected,
        .disconnected = battery_bt_disconnected,
};

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);
static const struct device* dev = DEVICE_DT_GET(DT_INST(0, maxim_max17048));

static uint8_t battery_level;
volatile bool notify_enable;
struct bt_conn* connection;

static void battery_init() {
    if (!device_is_ready(dev)) {
        LOG_ERR("MAX17048 not ready.");
        module_set_state(MODULE_STATE_ERROR);
        return;
    }

    notify_enable = false;
    // Wait a second before getting the first reading because it takes that long for the MAX17048 to be ready.
    k_timer_start(&battery_timer, K_SECONDS(1), K_SECONDS(60));

    module_set_state(MODULE_STATE_READY);
}

static bool battery_app_event_handler(const struct app_event_header* aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event* event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
            battery_init();
        }
    }

    return false;
}

static void battery_timer_update(struct k_timer* timer_id) {
    ARG_UNUSED(timer_id);
    k_work_submit(&battery_timer_work);
}

static void battery_timer_work_handler(struct k_work* work) {
    ARG_UNUSED(work);

    int rc = sensor_sample_fetch(dev);
    if (rc != 0) {
        LOG_WRN("Unable to fetch fuel gauge data");
        return;
    }

    struct sensor_value soc;
    rc = sensor_channel_get(dev, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &soc);
    if (rc != 0) {
        LOG_WRN("Unable to get battery state of charge");
        return;
    }

    battery_level = (uint8_t) soc.val1;

    if (connection && notify_enable) {
        rc = bt_gatt_notify(connection, &battery_svc.attrs[2], &battery_level, sizeof(battery_level));

        if (rc != 0) {
            LOG_ERR("Notify error: %d", rc);
        }
    }
}

static ssize_t
battery_bt_read_level(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &battery_level, sizeof(battery_level));
}

static void battery_bt_ccc_cfg_changed(const struct bt_gatt_attr* attr, uint16_t value) {
    notify_enable = (value == BT_GATT_CCC_NOTIFY);

    if (connection && notify_enable) {
        int rc = bt_gatt_notify(connection, attr, &battery_level, sizeof(battery_level));

        if (rc != 0) {
            LOG_ERR("Notify error: %d", rc);
        }
    }
}

static void battery_bt_connected(struct bt_conn* connected, uint8_t err) {
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
    } else {
        if (!connection) {
            connection = bt_conn_ref(connected);
        }
    }
}

static void battery_bt_disconnected(struct bt_conn* disconn, uint8_t reason) {
    ARG_UNUSED(disconn);
    ARG_UNUSED(reason);

    if (connection) {
        bt_conn_unref(connection);
        connection = NULL;
    }
}
#endif
