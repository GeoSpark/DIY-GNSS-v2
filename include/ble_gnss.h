#ifndef DIY_GNSS_V2_BLE_GNSS_H
#define DIY_GNSS_V2_BLE_GNSS_H

#include <zephyr/bluetooth/gatt.h>

#define BT_UUID_GNSS         BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xc33a0000, 0xbda8, 0x4293, 0xb836, 0x10dd6d78e7a1))
#define BT_UUID_LATITUDE     BT_UUID_DECLARE_16(0x2aae)
#define BT_UUID_LONGITUDE    BT_UUID_DECLARE_16(0x2aaf)
#define BT_UUID_ALTITUDE     BT_UUID_DECLARE_16(0x2ab3)
#define BT_UUID_ANT_HEIGHT   BT_UUID_DECLARE_16(0x2b47)

static const struct bt_gatt_cpf decimal_degrees_double = {
        .name_space = 1,
        .description = 0x0000,
        .exponent = 0,
        .format = 0x15,  // Double precision
        .unit = 0x2763   // Plane angle (degree)
};

static const struct bt_gatt_cpf metres_single = {
        .name_space = 1,
        .description = 0x0000,
        .exponent = 0,
        .format = 0x14,  // Single precision
        .unit = 0x2701   // Length (metre)
};

#endif //DIY_GNSS_V2_BLE_GNSS_H
