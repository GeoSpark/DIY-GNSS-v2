#include "max17048.h"

#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>

#define LOG_LEVEL CONFIG_MAX17048_LOG_LEVEL
#define DT_DRV_COMPAT maxim_max17048


#define REG_VCELL        0x02
#define REG_SOC          0x04
#define REG_MODE         0x06
#define REG_VERSION      0x08
#define REG_HIBRT        0x0a
#define REG_CONFIG       0x0c
#define REG_VALRT        0x14
#define REG_CRATE        0x16
#define REG_VRESET_ID    0x18
#define REG_STATUS       0x1a
#define REG_TABLE_START  0x40
#define REG_TABLE_END    0x7f
#define REG_CMD          0xfe

#define MODE_HIB_STAT    0x0010
#define MODE_EN_SLEEP    0x0020
#define MODE_QUICK_START 0x0040

#define STATUS_RI   0x0001
#define STATUS_VH   0x0002
#define STATUS_VL   0x0004
#define STATUS_VR   0x0008
#define STATUS_HD   0x0010
#define STATUS_SC   0x0020
#define STATUS_ENVR 0x0040

#define CMD_POR     0x5400

LOG_MODULE_REGISTER(MAX17048, LOG_LEVEL);

static int max17048_sample_fetch(const struct device *dev, enum sensor_channel chan);

static int max17048_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val);

static const struct sensor_driver_api max17048_api = {
        .sample_fetch = &max17048_sample_fetch,
        .channel_get = &max17048_channel_get
};


struct max17048_dev_data_t {
    /* Current cell voltage in units of 78.125uV */
    uint16_t voltage;
    /* Remaining capacity as a 1/256 percentage */
    uint16_t state_of_charge;
    // Approximate charge or discharge rate of the battery. 0.208%/hr
    uint16_t charge_rate;
    // Indicates overvoltage, undervoltage, SOC change, SOC low, and reset alerts.
    uint16_t status;
};

struct max17048_dev_cfg_t {
    struct i2c_dt_spec i2c;
};

static int max17048_reg_read(const struct device *dev, int reg_addr, int16_t *valp) {
    const struct max17048_dev_cfg_t *config = dev->config;
    uint8_t i2c_data[2];
    int rc;

    rc = i2c_burst_read_dt(&config->i2c, reg_addr, i2c_data, 2);
    if (rc < 0) {
        LOG_ERR("Unable to read register");
        return rc;
    }
    *valp = (i2c_data[0] << 8) | i2c_data[1];

    return 0;
}

static int max17048_reg_write(const struct device *dev, int reg_addr, uint16_t val) {
    const struct max17048_dev_cfg_t *config = dev->config;
    uint8_t buf[3];

    buf[0] = (uint8_t) reg_addr;
    sys_put_le16(val, &buf[1]);

    return i2c_write_dt(&config->i2c, buf, sizeof(buf));
}

static int max17048_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    ARG_UNUSED(chan);

    struct max17048_dev_data_t *data = dev->data;

    int ret = max17048_reg_read(dev, REG_VCELL, &data->voltage);

    if (ret != 0) {
        LOG_ERR("Failed to read VCELL");
        return ret;
    }

    ret = max17048_reg_read(dev, REG_SOC, &data->state_of_charge);

    if (ret != 0) {
        LOG_ERR("Failed to read REG_SOC");
        return ret;
    }

    ret = max17048_reg_read(dev, REG_CRATE, &data->charge_rate);

    if (ret != 0) {
        LOG_ERR("Failed to read REG_CRATE");
        return ret;
    }

    ret = max17048_reg_read(dev, REG_STATUS, &data->status);

    if (ret != 0) {
        LOG_ERR("Failed to read REG_STATUS");
        return ret;
    }

    return 0;
}

static int max17048_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val) {
    struct max17048_dev_data_t *data = dev->data;
    uint32_t tmp;

    switch (chan) {
        case SENSOR_CHAN_GAUGE_VOLTAGE:
            tmp = (data->voltage * 1250) / 16;
            val->val1 = tmp / 1000000;
            val->val2 = tmp % 1000000;
            break;
        case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
            val->val1 = data->state_of_charge >> 8;
            val->val2 = ((data->state_of_charge & 0xff) * 1000000) >> 8;
            break;
        case SENSOR_CHAN_GAUGE_CHARGE_RATE:
            val->val1 = data->charge_rate / 208;
            val->val2 = ((data->state_of_charge % 208) * 1000000) / 208;
            break;
        default:
            LOG_WRN("Invalid sensor channel %i", chan);
            return -EINVAL;
    }

    return 0;
}

static int max17048_init(const struct device *dev) {
    int16_t tmp;
    const struct max17048_dev_cfg_t *const config = dev->config;

    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    if (max17048_reg_read(dev, REG_STATUS, &tmp)) {
        return -EIO;
    }

    return 0;
}

#define MAX17048_DEFINE(inst)                                         \
    static struct max17048_dev_data_t max17048_data_##inst = {        \
    };                                                                  \
    static const struct max17048_dev_cfg_t max17048_cfg_##inst = {    \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                            \
    };                                                                \
    DEVICE_DT_INST_DEFINE(inst,                                       \
                          max17048_init,                              \
                          NULL,                                       \
                          &max17048_data_##inst,                      \
                          &max17048_cfg_##inst,                       \
                          POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,   \
                          &max17048_api);

DT_INST_FOREACH_STATUS_OKAY(MAX17048_DEFINE)
