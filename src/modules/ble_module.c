#include "ble_gnss.h"

#define MODULE ble_gnss
#include <caf/events/module_state_event.h>
#include <caf/events/ble_common_event.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

static double latitude = 23.0;
static double longitude = 42.0;
static float elevation = 69.0f;
static float antenna_height = 16.0f;

static ssize_t read_data(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    void* val = NULL;
    uint16_t val_len = 0;

    if (bt_uuid_cmp(attr->uuid, BT_UUID_LATITUDE) == 0) {
        val = &latitude;
        val_len = sizeof(latitude);
    } else if (bt_uuid_cmp(attr->uuid, BT_UUID_LONGITUDE) == 0) {
        val = &longitude;
        val_len = sizeof(longitude);
    } else if (bt_uuid_cmp(attr->uuid, BT_UUID_ALTITUDE) == 0) {
        val = &elevation;
        val_len = sizeof(elevation);
    } else if (bt_uuid_cmp(attr->uuid, BT_UUID_ANT_HEIGHT) == 0) {
        val = &antenna_height;
        val_len = sizeof(antenna_height);
    } else {
        LOG_ERR("Trying to read unknown value");
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, val, val_len);
}

static ssize_t write_latitude(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    latitude = *(double*)buf;

    LOG_DBG("Latitude: %f", latitude);

    return len;
}

static void init_fn(void) {
    module_set_state(MODULE_STATE_READY);
}

static bool handle_ble_peer_event(const struct ble_peer_event* event) {
    switch (event->state) {
        case PEER_STATE_DISCONNECTED:
            break;
        case PEER_STATE_DISCONNECTING:
            break;
        case PEER_STATE_CONNECTED:
            break;
        case PEER_STATE_SECURED:
            break;
        case PEER_STATE_CONN_FAILED:
            break;
        case PEER_STATE_COUNT:
            break;
    }

    return true;
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

    if (is_ble_peer_event(aeh)) {
        return handle_ble_peer_event(cast_ble_peer_event(aeh));
    }

    return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, ble_peer_event);

BT_GATT_SERVICE_DEFINE(GNSS,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_GNSS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LATITUDE,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_data, write_latitude, NULL),
                       BT_GATT_CUD("Latitude of base station", BT_GATT_PERM_READ),
                       BT_GATT_CPF(&decimal_degrees_double),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LONGITUDE,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_data, write_latitude, NULL),
                       BT_GATT_CUD("Longitude of base station", BT_GATT_PERM_READ),
                       BT_GATT_CPF(&decimal_degrees_double),
                       BT_GATT_CHARACTERISTIC(BT_UUID_ALTITUDE,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_data, write_latitude, NULL),
                       BT_GATT_CUD("Elevation of base station", BT_GATT_PERM_READ),
                       BT_GATT_CPF(&metres_single),
                       BT_GATT_CHARACTERISTIC(BT_UUID_ANT_HEIGHT,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_data, write_latitude, NULL),
                       BT_GATT_CUD("Height of antenna", BT_GATT_PERM_READ),
                       BT_GATT_CPF(&metres_single),
                       );
