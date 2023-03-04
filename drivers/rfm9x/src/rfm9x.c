#include "rfm9x.h"

#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/lora.h>


#define LOG_LEVEL CONFIG_RFM9x_LOG_LEVEL
#define DT_DRV_COMPAT hoperf_rfm9x

LOG_MODULE_REGISTER(RFM9x, LOG_LEVEL);


struct rfm9x_dev_data_t {
    uint16_t voltage;
};

struct rfm9x_dev_cfg_t {
    struct spi_dt_spec spi;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec enable_gpio;
};

static const struct lora_driver_api rfm9x_api = {};

static int rfm9x_init(const struct device *dev) {
    const struct rfm9x_dev_cfg_t *const config = dev->config;

    if (!spi_is_ready(&config->spi)) {
        LOG_ERR("SPI bus not ready");
        return -ENODEV;
    }

    if (config->reset_gpio.port != NULL) {
        if (!device_is_ready(config->reset_gpio.port)) {
            LOG_ERR("Reset GPIO device not ready");
            return -ENODEV;
        }
//
//        if (gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE)) {
//            LOG_ERR("Couldn't configure reset pin");
//            return -EIO;
//        }
    }

    if (config->enable_gpio.port != NULL) {
        if (!device_is_ready(config->enable_gpio.port)) {
            LOG_ERR("Enable GPIO device not ready");
            return -ENODEV;
        }

//        if (gpio_pin_configure_dt(&config->enable_gpio, GPIO_OUTPUT_INACTIVE)) {
//            LOG_ERR("Couldn't configure enable pin");
//            return -EIO;
//        }
    }

    LOG_INF("WIBBLE!");

    return 0;
}

#define RFM9x_DEFINE(inst)                                         \
    static struct rfm9x_dev_data_t rfm9x_data_##inst = {           \
    };                                                             \
    static const struct rfm9x_dev_cfg_t rfm9x_cfg_##inst = {       \
        .spi = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8) | SPI_OP_MODE_MASTER, 0),                        \
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, rst_gpios, {}),		\
		.enable_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, en_gpios, {}),		\
    };                                                             \
    DEVICE_DT_INST_DEFINE(inst,                                    \
                          rfm9x_init,                              \
                          NULL,                                    \
                          &rfm9x_data_##inst,                      \
                          &rfm9x_cfg_##inst,                       \
                          POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,\
                          &rfm9x_api);

DT_INST_FOREACH_STATUS_OKAY(RFM9x_DEFINE)
