#define MODULE data_module

#include "events/data_event.h"
#include "config.h"

#include <caf/events/module_state_event.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <bluetooth/adv_prov.h>

#define BT_UUID_GNSS                BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a0000, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define BT_UUID_CONFIG              BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a0001, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define DEVICE_SETTINGS_KEY         "gnss"
#define DEVICE_SETTINGS_CONFIG_KEY  "config"

static ssize_t bt_read_data(struct bt_conn*, const struct bt_gatt_attr*, void*, uint16_t, uint16_t);

static ssize_t bt_write_data(struct bt_conn*, const struct bt_gatt_attr*, const void*, uint16_t, uint16_t, uint8_t);

static int load_config(const char*, size_t, settings_read_cb, void*);

static bool save_config(struct config_t);

static bool app_event_handler(const struct app_event_header*);

static int get_data(struct bt_data*, const struct bt_le_adv_prov_adv_state*, struct bt_le_adv_prov_feedback*);

static struct bt_data BT_UUID_ADV = BT_DATA_BYTES(BT_DATA_UUID128_SOME, BT_UUID_128_ENCODE(0xc33a0000, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1));

static struct config_t config = {.latitude = 0.0, .longitude = 0.0, .elevation = 0.0f, .antenna_height = 0.0f, .sample_interval = 30};

SETTINGS_STATIC_HANDLER_DEFINE(MODULE, DEVICE_SETTINGS_KEY, NULL, load_config, NULL, NULL);

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, rover_event);

BT_GATT_SERVICE_DEFINE(svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_GNSS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_CONFIG,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                                              bt_read_data,
                                              bt_write_data,
                                              NULL),
                       BT_GATT_CUD("Config", BT_GATT_PERM_READ),
);

BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(service_uuids, get_data);

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

static ssize_t
bt_read_data(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
    void* val = NULL;
    uint16_t val_len;

    if (bt_uuid_cmp(attr->uuid, BT_UUID_CONFIG) == 0) {
        val = &config;
        val_len = sizeof(config);
        LOG_DBG("Config read %d bytes at offset %d", len, offset);
    } else {
        LOG_ERR("Trying to read unknown value expecting config");
        return 0;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, val, val_len);
}

static ssize_t
bt_write_data(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len,
                   uint16_t offset,
                   uint8_t flags) {
    ARG_UNUSED(conn);
    ARG_UNUSED(offset);
    ARG_UNUSED(flags);

    if (bt_uuid_cmp(attr->uuid, BT_UUID_CONFIG) != 0) {
        LOG_ERR("Trying to write unknown value to config");
        return 0;
    }

    static struct config_t config_buffer;

    uint8_t* conf_buf = (uint8_t*) &config_buffer;

    memcpy(&conf_buf[offset], buf, len);

    if (offset + len >= sizeof(struct config_t)) {
        save_config(config_buffer);
    }

    return len;
}

static int load_config(const char* name, size_t len, settings_read_cb read_cb, void* cb_arg) {
    ARG_UNUSED(len);

    int err = 0;

    if (strcmp(name, DEVICE_SETTINGS_CONFIG_KEY) == 0) {
        err = read_cb(cb_arg, &config, sizeof(config));
        if (err < 0) {
            LOG_ERR("Failed to load configuration, error: %d", err);
        } else {
            LOG_DBG("Device configuration loaded from flash");
            err = 0;
        }
    }

    return err;
}

static bool save_config(struct config_t cfg) {
    if (cfg.latitude < -90.0 || cfg.latitude > 90.0 ||
        cfg.longitude < -180.0 || cfg.longitude > 180.0 ||
        cfg.elevation < -10000.0f || cfg.elevation > 9000.0f ||
        cfg.antenna_height < 0.0f ||
        cfg.sample_interval < 1) {
        return false;
    }

    settings_save_one(DEVICE_SETTINGS_KEY"/"DEVICE_SETTINGS_CONFIG_KEY, &cfg, sizeof(struct config_t));

    struct data_event* config_event = new_data_event();
    config_event->config = config;
    APP_EVENT_SUBMIT(config_event);

    return true;
}

static int init_settings() {
    int err;

    err = settings_subsys_init();
    if (err != 0) {
        LOG_ERR("settings_subsys_init, error: %d", err);
        return err;
    }

    err = settings_load_subtree(DEVICE_SETTINGS_KEY);
    if (err != 0) {
        LOG_ERR("Failed to read config");
        return err;
    }

    return 0;
}

static bool app_event_handler(const struct app_event_header* aeh) {
    if (is_module_state_event(aeh)) {
        struct module_state_event* event = cast_module_state_event(aeh);

        if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
            int err = init_settings();

            if (err != 0) {
                module_set_state(MODULE_STATE_ERROR);
                return false;
            }

            module_set_state(MODULE_STATE_READY);

            struct data_event* config_event = new_data_event();
            config_event->config = config;
            APP_EVENT_SUBMIT(config_event);
        }
    }

    return false;
}

// Callback used by BT_LE_ADV_PROV_AD_PROVIDER_REGISTER. It has to be called get_data().
static int
get_data(struct bt_data* ad, const struct bt_le_adv_prov_adv_state* state, struct bt_le_adv_prov_feedback* fb) {
    ARG_UNUSED(state);
    ARG_UNUSED(fb);

    ad->type = BT_UUID_ADV.type;
    ad->data_len = BT_UUID_ADV.data_len;
    ad->data = BT_UUID_ADV.data;

    return 0;
}
