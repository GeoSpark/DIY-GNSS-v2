#include "px1122r.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#define LOG_LEVEL CONFIG_PX1122R_LOG_LEVEL
#define DT_DRV_COMPAT skytraq_px1122r

LOG_MODULE_REGISTER(PX1122R, LOG_LEVEL);

static void print_impl(const struct device *dev);

__subsystem struct px1122r_driver_api {
	void (*print)(const struct device *dev);
};

static const struct px1122r_driver_api px1122r_api = {
	.print = print_impl
};

struct px1122r_dev_data {
	uint32_t foo;
};

struct px1122r_dev_cfg {
	uint32_t flange;
};

static int init(const struct device *dev)
{
	return 0;
}

void px1122r_print(const struct device *dev) {
	const struct px1122r_driver_api *api = dev->api;

	__ASSERT(api->print, "Callback pointer should not be NULL");

	api->print(dev);
}

static void print_impl(const struct device *dev)
{
	uint32_t val = ((struct px1122r_dev_data*)dev->data)->foo;
	LOG_INF("Hello World from the kernel: %d", val);

	__ASSERT(data.foo == 5, "Device was not initialized!");
}

#define PX1122R_DEFINE(inst)                                               \
	static struct px1122r_dev_data px1122r_data_##inst = {                 \
		.foo = 5,				                                           \
	};									                                   \
	static const struct px1122r_dev_cfg px1122r_cfg_##inst = {             \
		.flange = 1,                                                       \
	};                                                                     \
	DEVICE_DT_INST_DEFINE(inst,                                            \
						  init,                                            \
						  NULL,                                            \
						  &px1122r_data_##inst,                            \
						  &px1122r_cfg_##inst,                             \
						  POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,   \
						  &px1122r_api);

DT_INST_FOREACH_STATUS_OKAY(PX1122R_DEFINE)
