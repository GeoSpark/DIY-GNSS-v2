#include "px1122r.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#define LOG_LEVEL CONFIG_PX1122R_LOG_LEVEL
#define DT_DRV_COMPAT skytraq_px1122r

LOG_MODULE_REGISTER(PX1122R, LOG_LEVEL);

static void print_impl(const struct device *dev);

static const struct px1122r_driver_api px1122r_api = {
	.print = print_impl
};


static struct px1122r_dev_data {
	uint32_t foo;
} data;

static int init(const struct device *dev)
{
	// data.foo = 5;

	return 0;
}

void px1122r_print(const struct device *dev) {
	const struct px1122r_driver_api *api = dev->api;

	__ASSERT(api->print, "Callback pointer should not be NULL");

	api->print(dev);
}

static void print_impl(const struct device *dev)
{
	LOG_INF("Hello World from the kernel: %d", data.foo);

	__ASSERT(data.foo == 5, "Device was not initialized!");
}

#define PX1122R_DEFINE(inst)							\
	static struct px1122r_dev_data px1122r_dev_data_##inst = {			\
		.foo = 5,				\
	};									\
	DEVICE_DT_INST_DEFINE(inst,						\
						  init, NULL,                              \
						  &px1122r_dev_data_##inst, NULL, \
						  POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,             \
						  &px1122r_api);


	// static const struct counter_config_info pcf85063_cfg_info_##inst = {	\
	// 	.max_top_value = 0xff,						\
	// 	.freq = 1,							\
	// 	.channels = 1,							\
	// };									\

/* Create the struct device for every status "okay"*/
DT_INST_FOREACH_STATUS_OKAY(PX1122R_DEFINE)
