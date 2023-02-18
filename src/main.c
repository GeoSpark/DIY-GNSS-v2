#include "px1122r.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(DIY_GNSS_V2, LOG_LEVEL_DBG);
const struct device* dev = DEVICE_DT_GET(DT_INST(0, skytraq_px1122r));

void main(void) {
#if (__ASSERT_ON == 0)
	if (dev == NULL) {
		LOG_ERR("Failed to get device binding for PX1122R");
		return;
	}
#endif
	__ASSERT(dev, "Failed to get device binding");

	LOG_INF("Initializing device %s (@%p)", dev->name, dev);
	px1122r_print(dev);
}
