#ifndef _PX1122R_H_
#define _PX1122R_H_

#include <zephyr/device.h>

__subsystem struct px1122r_driver_api {
	/* This struct has a member called 'print'. 'print' is function
	 * pointer to a function that takes 'struct device *dev' as an
	 * argument and returns 'void'.
	 */
	void (*print)(const struct device *dev);
};

// __syscall     void        px1122r_print(const struct device *dev);
// static inline void z_impl_px1122r_print(const struct device *dev)
// {
// 	const struct px1122r_driver_api *api = dev->api;

// 	__ASSERT(api->print, "Callback pointer should not be NULL");

// 	api->print(dev);
// }

// #include <syscalls/px1122r.h>

void px1122r_print(const struct device *dev);

#endif
