#ifndef DIY_GNSS_V2_CONFIG_H
#define DIY_GNSS_V2_CONFIG_H

#include <zephyr/kernel.h>

struct config_t {
    double latitude;
    double longitude;
    float elevation;
    float antenna_height;
    uint8_t sample_interval;
};

#endif //DIY_GNSS_V2_CONFIG_H
