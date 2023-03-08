#ifndef _PX1122R_H_
#define _PX1122R_H_

#include <zephyr/device.h>

int px1122r_start_stream(const struct device* dev);

int px1122r_stop_stream(const struct device* dev);

int px1122r_send_command(const struct device* dev);

#endif
