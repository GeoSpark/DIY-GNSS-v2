#ifndef _PX1122R_H_
#define _PX1122R_H_

#include <zephyr/device.h>

typedef void (*px1122r_callback_t)(const uint8_t* buf, const uint8_t len);

int px1122r_start_stream(const struct device* dev, px1122r_callback_t cb);

int px1122r_stop_stream(const struct device* dev);

int px1122r_send_command(const struct device* dev, const void* command, const uint16_t length);

struct px1122r_cmd_rtk_mode_t {
    uint8_t msg_id;
    uint8_t msg_sub_id;
    uint8_t mode;
    uint8_t function;
    uint32_t survey_length;
    uint32_t std_dev;
    double latitude;
    double longitude;
    float altitude;
    float baseline_length;
    uint8_t attributes;
} __attribute__((packed));

struct px1122r_cmd_psti_interval_t {
    uint8_t msg_id;
    uint8_t msg_sub_id;
    uint8_t psti_id;
    uint8_t interval;
    uint8_t attributes;
} __attribute__((packed));

struct px1122r_cmd_nmea_string_interval_t {
    uint8_t msg_id;
    uint8_t msg_sub_id;
    uint8_t nmea_id[3];
    uint8_t interval;
    uint8_t attributes;
} __attribute__((packed));

enum px1122r_talker_id_t {
    TALKER_ID_GP_MODE,
    TALKER_ID_GN_MODE,
    TALKER_ID_AUTO_MODE
};

struct px1122r_cmd_nmea_talker_id_t {
    uint8_t msg_id;
    uint8_t talker_id;
    uint8_t attributes;
} __attribute__((packed));

struct px1122r_cmd_message_type_t {
    uint8_t msg_id;
    uint8_t type;
    uint8_t attributes;
} __attribute__((packed));

#endif
