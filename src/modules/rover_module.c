#define MODULE rover

#include <caf/events/module_state_event.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <bluetooth/adv_prov.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

#define BT_UUID_ROVER        BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a1000, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define BT_UUID_ROVER_CONFIG BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a1001, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define BT_UUID_ROVER_ACTIVE BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a1002, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))

static struct rover_config_t {
    float antenna_height;
    uint8_t sample_interval;
} rover_config = {.antenna_height = 0.0f, .sample_interval = 1};

static bool rover_active = false;

static ssize_t rover_bt_is_active(struct bt_conn*, const struct bt_gatt_attr*, void*, uint16_t, uint16_t);

static ssize_t rover_bt_activate(struct bt_conn*, const struct bt_gatt_attr*, const void*, uint16_t, uint16_t, uint8_t);

static bool rover_app_event_handler(const struct app_event_header*);

static ssize_t rover_bt_read_data(struct bt_conn*, const struct bt_gatt_attr*, void*, uint16_t, uint16_t);

static ssize_t
rover_bt_write_data(struct bt_conn*, const struct bt_gatt_attr*, const void*, uint16_t, uint16_t, uint8_t);

static int rover_load_config(const char*, size_t, settings_read_cb, void*);

static int rover_config_loaded();

APP_EVENT_LISTENER(MODULE, rover_app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);

BT_GATT_SERVICE_DEFINE(rover_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_ROVER),
                       BT_GATT_CHARACTERISTIC(BT_UUID_ROVER_CONFIG,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, rover_bt_read_data,
                                              rover_bt_write_data,
                                              NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_ROVER_ACTIVE,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, rover_bt_is_active, rover_bt_activate,
                                              NULL),
                       BT_GATT_CUD("Rover config", BT_GATT_PERM_READ),
);

struct settings_handler rover_conf = {
        .name = "rover",
        .h_set = rover_load_config,
        .h_commit = rover_config_loaded,
};

static void rover_activate(bool status) {
    rover_active = status;
    // TODO: Send message.
}

static ssize_t
rover_bt_is_active(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
    if (bt_uuid_cmp(attr->uuid, BT_UUID_ROVER_ACTIVE) != 0) {
        LOG_ERR("Unable to read rover activity status");
        return 0;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &rover_active, sizeof(rover_active));
}

static ssize_t
rover_bt_activate(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len, uint16_t offset,
                 uint8_t flags) {
    ARG_UNUSED(conn);
    ARG_UNUSED(offset);
    ARG_UNUSED(flags);

    if (bt_uuid_cmp(attr->uuid, BT_UUID_ROVER_ACTIVE) != 0) {
        LOG_ERR("Trying to write unknown value to rover config");
        return 0;
    }

    rover_activate(*(bool*) buf);
    return len;
}

static ssize_t
rover_bt_read_data(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
    void* val = NULL;
    uint16_t val_len;

    if (bt_uuid_cmp(attr->uuid, BT_UUID_ROVER_CONFIG) == 0) {
        int ret = settings_load_subtree("rover/config");
        if (ret != 0) {
            LOG_ERR("Failed to read rover config");
            return 0;
        }

        val = &rover_config;
        val_len = sizeof(struct rover_config_t);
        LOG_DBG("Rover config read");
    } else {
        LOG_ERR("Trying to read unknown value expecting rover config");
        return 0;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, val, val_len);
}

static ssize_t
rover_bt_write_data(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len,
                    uint16_t offset,
                    uint8_t flags) {
    ARG_UNUSED(conn);
    ARG_UNUSED(offset);
    ARG_UNUSED(flags);

    if (bt_uuid_cmp(attr->uuid, BT_UUID_ROVER_CONFIG) != 0) {
        LOG_ERR("Trying to write unknown value to rover config");
        return 0;
    }

    if (len != sizeof(struct rover_config_t)) {
        LOG_ERR("Trying to write wrong number of bytes to rover config");
        return 0;
    }

    settings_save_one("rover/config", buf, len);

    return len;
}

static int rover_load_config(const char* name, size_t len, settings_read_cb read_cb, void* cb_arg) {
    const char* next;
    int rc;

    if (settings_name_steq(name, "config", &next) && !next) {
        if (len != sizeof(struct rover_config_t)) {
            LOG_ERR("Rover config size has changed");
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &rover_config, sizeof(struct rover_config_t));

        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    return -ENOENT;
}

static int rover_config_loaded() {
    // TODO: All settings are loaded. Do something with them here.
    return 0;
}


static void rover_init(void) {
    settings_register(&rover_conf);
    module_set_state(MODULE_STATE_READY);
}

static bool rover_app_event_handler(const struct app_event_header* aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event* event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
            rover_init();
        }
    }

    return false;
}
