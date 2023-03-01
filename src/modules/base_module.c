#define MODULE base

#include <caf/events/module_state_event.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <bluetooth/adv_prov.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

#define BT_UUID_BASE         BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a0000, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define BT_UUID_BASE_CONFIG  BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a0001, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define BT_UUID_BASE_ACTIVE  BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a0002, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))

static struct bt_data BT_UUID_BASE_ADV = BT_DATA_BYTES(BT_DATA_UUID128_SOME,
                                                       BT_UUID_128_ENCODE(0xc33a0000, 0xbda8, 0x4293, 0xb836,
                                                                          0x10dd6d78e7a1));

static struct base_config_t {
    double latitude;
    double longitude;
    float elevation;
    float antenna_height;
    uint8_t sample_interval;
} base_config = {.latitude = 0.0, .longitude = 0.0, .elevation = 0.0f, .antenna_height = 0.0f, .sample_interval = 30};

static bool _active = false;

static void activate(bool status) {
    _active = status;
    // TODO: Send message.
}

static bool configure(struct base_config_t config) {
    if (config.latitude < -90.0 || config.latitude > 90.0 ||
        config.longitude < -180.0 || config.longitude > 180.0 ||
        config.antenna_height < 0.0f ||
        config.sample_interval < 1) {
        return false;
    }

    settings_save_one("base/config", &config, sizeof(struct base_config_t));
    return true;
}

static ssize_t
ble_is_active(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    if (bt_uuid_cmp(attr->uuid, BT_UUID_BASE_ACTIVE) != 0) {
        LOG_ERR("Unable to read base activity status");
        return 0;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &_active, sizeof(_active));
}

static ssize_t
ble_activate(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset,
             uint8_t flags) {
    ARG_UNUSED(conn);
    ARG_UNUSED(offset);
    ARG_UNUSED(flags);

    if (bt_uuid_cmp(attr->uuid, BT_UUID_BASE_ACTIVE) != 0) {
        LOG_ERR("Trying to write unknown value to base config");
        return 0;
    }

    activate(*(bool *) buf);
    return len;
}

static ssize_t
read_base_data(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    void *val = NULL;
    uint16_t val_len;

    if (bt_uuid_cmp(attr->uuid, BT_UUID_BASE_CONFIG) == 0) {
        int ret = settings_load_subtree("base/config");
        if (ret != 0) {
            LOG_ERR("Failed to read base config");
            return 0;
        }

        val = &base_config;
        val_len = sizeof(base_config);
        LOG_DBG("Base config read %d bytes at offset %d", len, offset);
    } else {
        LOG_ERR("Trying to read unknown value expecting base config");
        return 0;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, val, val_len);
}

static ssize_t
write_base_data(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset,
                uint8_t flags) {
    ARG_UNUSED(conn);
    ARG_UNUSED(offset);
    ARG_UNUSED(flags);

    if (bt_uuid_cmp(attr->uuid, BT_UUID_BASE_CONFIG) != 0) {
        LOG_ERR("Trying to write unknown value to base config");
        return 0;
    }

    static struct base_config_t config_buffer;

    uint8_t* conf_buf = (uint8_t*)&config_buffer;

    memcpy(&conf_buf[offset], buf, len);

    if (offset + len >= sizeof(struct base_config_t)) {
        configure(config_buffer);
    }

    return len;
}

static int load_base_config(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    if (settings_name_steq(name, "config", &next) && !next) {
        if (len != sizeof(base_config)) {
            LOG_ERR("Base config size has changed");
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &base_config, sizeof(base_config));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    return -ENOENT;
}

static int base_config_loaded() {
    // TODO: All settings are loaded. Do something with them here.
    return 0;
}

struct settings_handler base_conf = {
        .name = "base",
        .h_set = load_base_config,
        .h_commit = base_config_loaded,
};

static void init_fn(void) {
    settings_register(&base_conf);
    module_set_state(MODULE_STATE_READY);
}

static bool app_event_handler(const struct app_event_header *aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event *event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
            static bool initialized = false;

            if (initialized) {
                LOG_WRN("Module \"base\" is being initialised twice. Skipping.");
            } else {
                initialized = true;
                init_fn();
            }
        }
    }

    return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);

BT_GATT_SERVICE_DEFINE(base,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_BASE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_BASE_CONFIG,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_base_data, write_base_data,
                                              NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_BASE_ACTIVE,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, ble_is_active, ble_activate,
                                              NULL),
                       BT_GATT_CUD("Base config", BT_GATT_PERM_READ),
);

static int
get_data(struct bt_data *ad, const struct bt_le_adv_prov_adv_state *state, struct bt_le_adv_prov_feedback *fb) {
    ARG_UNUSED(state);
    ARG_UNUSED(fb);

    ad->type = BT_UUID_BASE_ADV.type;
    ad->data_len = BT_UUID_BASE_ADV.data_len;
    ad->data = BT_UUID_BASE_ADV.data;

    return 0;
}

BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(service_uuids, get_data);
