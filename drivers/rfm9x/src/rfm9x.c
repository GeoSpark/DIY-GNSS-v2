#include "rfm9x.h"

#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/lora.h>


#define LOG_LEVEL CONFIG_RFM9x_LOG_LEVEL
#define DT_DRV_COMPAT hoperf_rfm9x

#define REG_SPI_WRITE_BIT        BIT(7)

struct rfm9x_dev_data_t {
    uint16_t voltage;
};

struct rfm9x_dev_cfg_t {
    struct spi_dt_spec bus;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec enable_gpio;
};

static int rfm9x_lora_config(const struct device* dev, struct lora_modem_config* config);

static int rfm9x_init(const struct device* dev);

static const struct lora_driver_api rfm9x_api = {
        .config = rfm9x_lora_config
};

#define RFM9x_DEFINE(inst)                                         \
    static struct rfm9x_dev_data_t rfm9x_data_##inst = {           \
    };                                                             \
    static const struct rfm9x_dev_cfg_t rfm9x_cfg_##inst = {       \
        .bus = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB, 0),                        \
        .reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, rst_gpios, {}),        \
        .enable_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, en_gpios, {}),        \
    };                                                             \
    DEVICE_DT_INST_DEFINE(inst,                                    \
                          rfm9x_init,                              \
                          NULL,                                    \
                          &rfm9x_data_##inst,                      \
                          &rfm9x_cfg_##inst,                       \
                          POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,\
                          &rfm9x_api);

DT_INST_FOREACH_STATUS_OKAY(RFM9x_DEFINE)

LOG_MODULE_REGISTER(RFM9x, LOG_LEVEL);

void rfm9x_read_cb(const struct device *dev, int result, void *data) {}
void rfm9x_write_cb(const struct device *dev, int result, void *data) {}

static int rfm9x_write_register(const struct spi_dt_spec* bus, uint8_t reg, uint8_t data) {
    reg |= REG_SPI_WRITE_BIT;

    uint8_t buf[2] = {reg, data};

    const struct spi_buf tx_buf = {
            .buf = buf,
            .len = 2,
    };

    const struct spi_buf_set tx = {
            .buffers = &tx_buf,
            .count = 1,
    };

//    return spi_write_dt(bus, &tx);
    return spi_transceive_cb(bus->bus, &bus->config, &tx, NULL, rfm9x_write_cb, NULL);
}

static int rfm9x_read_register(const struct spi_dt_spec* bus, uint8_t reg, uint8_t* data) {
    const struct spi_buf tx_buf = {
            .buf = &reg,
            .len = 1,
    };

    const struct spi_buf_set tx = {
            .buffers = &tx_buf,
            .count = 1,
    };

    struct spi_buf rx_buf[2] = {
            {
                    .buf = NULL,
                    .len = 1,
            },
            {
                    .buf = data,
                    .len = 1,
            }
    };

    const struct spi_buf_set rx = {
            .buffers = rx_buf,
            .count = 2,
    };

    return spi_transceive_cb(bus->bus, &bus->config, &tx, &rx, rfm9x_read_cb, NULL);
}

static int rfm9x_init(const struct device* dev) {
    const struct rfm9x_dev_cfg_t* const spi = dev->config;

    if (!spi_is_ready(&spi->bus)) {
        LOG_ERR("SPI bus not ready");
        return -ENODEV;
    }

    if (spi->reset_gpio.port != NULL) {
        if (!device_is_ready(spi->reset_gpio.port)) {
            LOG_ERR("Reset GPIO device not ready");
            return -ENODEV;
        }

        if (gpio_pin_configure_dt(&spi->reset_gpio, GPIO_OUTPUT_INACTIVE)) {
            LOG_ERR("Couldn't configure reset pin");
            return -EIO;
        }
    }

    if (spi->enable_gpio.port != NULL) {
        if (!device_is_ready(spi->enable_gpio.port)) {
            LOG_ERR("Enable GPIO device not ready");
            return -ENODEV;
        }

        if (gpio_pin_configure_dt(&spi->enable_gpio, GPIO_OUTPUT_ACTIVE)) {
            LOG_ERR("Couldn't configure enable pin");
            return -EIO;
        }
    }

    int ret = rfm9x_write_register(&spi->bus, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE);
    if (ret < 0) {
        LOG_ERR("SPI write failed: %i", ret);
        return ret;
    }

    return 0;
}

static int rfm9x_lora_config(const struct device* dev, struct lora_modem_config* config) {
    const struct rfm9x_dev_cfg_t* const spi = dev->config;

    rfm9x_write_register(&spi->bus, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_STDBY | RH_RF95_LONG_RANGE_MODE);
    rfm9x_write_register(&spi->bus, RH_RF95_REG_0E_FIFO_TX_BASE_ADDR, 0);
    rfm9x_write_register(&spi->bus, RH_RF95_REG_0F_FIFO_RX_BASE_ADDR, 0);

    // Frequency
    uint32_t frf = config->frequency / RH_RF95_FSTEP;
    rfm9x_write_register(&spi->bus, RH_RF95_REG_06_FRF_MSB, (frf >> 16) & 0xff);
    rfm9x_write_register(&spi->bus, RH_RF95_REG_07_FRF_MID, (frf >> 8) & 0xff);
    rfm9x_write_register(&spi->bus, RH_RF95_REG_08_FRF_LSB, frf & 0xff);

    // Power
    int8_t power = MAX(MIN(config->tx_power, 23), 5);
    if (power > 20) {
        rfm9x_write_register(&spi->bus, RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_ENABLE);
        power -= 3;
    } else {
        rfm9x_write_register(&spi->bus, RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_DISABLE);
    }

    rfm9x_write_register(&spi->bus, RH_RF95_REG_09_PA_CONFIG, RH_RF95_PA_SELECT | (power - 5));

    // Preamble length
    rfm9x_write_register(&spi->bus, RH_RF95_REG_20_PREAMBLE_MSB, config->preamble_len >> 8);
    rfm9x_write_register(&spi->bus, RH_RF95_REG_21_PREAMBLE_LSB, config->preamble_len & 0xff);

    uint8_t conf1;
    rfm9x_read_register(&spi->bus, RH_RF95_REG_1D_MODEM_CONFIG1, &conf1);

    // Bandwidth
    switch (config->bandwidth) {
        case BW_125_KHZ:
            conf1 = (conf1 & ~RH_RF95_BW) | RH_RF95_BW_125KHZ;
            break;
        case BW_250_KHZ:
            conf1 = (conf1 & ~RH_RF95_BW) | RH_RF95_BW_250KHZ;
            break;
        case BW_500_KHZ:
            conf1 = (conf1 & ~RH_RF95_BW) | RH_RF95_BW_500KHZ;
            break;
    }

    // Coding rate
    uint8_t cr = config->coding_rate << 1;
    conf1 = (conf1 & ~RH_RF95_CODING_RATE) | cr;

    // Datarate
    conf1 = (conf1 & ~RH_RF95_SPREADING_FACTOR) | (config->datarate << 4);

    rfm9x_write_register(&spi->bus, RH_RF95_REG_1D_MODEM_CONFIG1, conf1);

    uint8_t conf2;
    rfm9x_read_register(&spi->bus, RH_RF95_REG_1D_MODEM_CONFIG1, &conf2);
    LOG_DBG("Expected: %x got %x", conf1, conf2);

    // TX/RX mode
    uint8_t opmode;
    rfm9x_read_register(&spi->bus, RH_RF95_REG_01_OP_MODE, &opmode);

    if (config->tx) {
        opmode = (opmode & ~RH_RF95_MODE) | RH_RF95_MODE_TX;
    } else {
        opmode = (opmode & ~RH_RF95_MODE) | RH_RF95_MODE_RXCONTINUOUS;
    }

    rfm9x_write_register(&spi->bus, RH_RF95_REG_01_OP_MODE, opmode);

    return 0;
}
