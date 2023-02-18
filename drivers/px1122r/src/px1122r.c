#include "px1122r.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/time_units.h>

#define LOG_LEVEL CONFIG_PX1122R_LOG_LEVEL
#define DT_DRV_COMPAT skytraq_px1122r
#define FRAME_SIZE 32

LOG_MODULE_REGISTER(PX1122R, LOG_LEVEL);

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data);

struct px1122r_dev_data {
	uint8_t foo;
	const struct device *uart_dev;
    // bool ready;
    uint8_t tx_buf[FRAME_SIZE * 2];
};

struct px1122r_dev_cfg {
	uint32_t instance;
};

static int px1122r_init(const struct device *dev) {
	LOG_INF("Initializing %s", dev->name);

	struct px1122r_dev_data* data = dev->data;
	data->uart_dev = DEVICE_DT_GET(DT_INST_BUS(0));

	if (!device_is_ready(data->uart_dev)) {
		return -ENODEV;
	}

	data->foo = 'A';

	int err = uart_callback_set(data->uart_dev, uart_cb, data);

	if (err) {
		LOG_ERR("Failed to init UART callback");
		return -EINVAL;
	}

	LOG_INF("%s initialized", dev->name);

	return 0;
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data) {
	// struct px1122r_dev_data* data = dev->data;

	LOG_DBG("evt->type %d", evt->type);

	switch (evt->type) {
		case UART_TX_DONE:
		LOG_DBG("Tx sent %d bytes", evt->data.tx.len);
		break;

		case UART_TX_ABORTED:
		LOG_ERR("Tx aborted");
		break;

		case UART_RX_RDY:
		break;

		case UART_RX_BUF_REQUEST:
		case UART_RX_BUF_RELEASED:
		case UART_RX_DISABLED:
		case UART_RX_STOPPED:
			break;
	}
}

void px1122r_send_data(const struct device *dev) {
	struct px1122r_dev_data* data = dev->data;
	data->tx_buf[0] = data->foo;
	data->tx_buf[1] = 0;
	LOG_DBG("Sending %s", data->tx_buf);
	uart_tx(data->uart_dev, data->tx_buf, 1, SYS_FOREVER_US);
}

#define PX1122R_DEFINE(inst)                                               \
	static struct px1122r_dev_data px1122r_data_##inst = {                 \
	};									                                   \
	static const struct px1122r_dev_cfg px1122r_cfg_##inst = {             \
		.instance = 0,                                                     \
	};                                                                     \
	DEVICE_DT_INST_DEFINE(inst,                                            \
						  px1122r_init,                                    \
						  NULL,                                            \
						  &px1122r_data_##inst,                            \
						  &px1122r_cfg_##inst,                             \
						  POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,   \
						  NULL);

DT_INST_FOREACH_STATUS_OKAY(PX1122R_DEFINE)
