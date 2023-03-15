#ifndef _PX1122R_H_
#define _PX1122R_H_

#include <zephyr/device.h>

typedef void (* px1122r_callback_t)(const uint8_t* buf, uint16_t len);

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
#define PX1122R_CMD_RTK_MODE(_mode, _function, _survey_length, _std_dev, _latitude, _longitude, _altitude, _baseline_length) \
    {.msg_id = 0x6a, .msg_sub_id = 0x06,                                                                                     \
    .mode = _mode,                                                                                                           \
    .function = _function,                                                                                                   \
    .survey_length = sys_cpu_to_be32(_survey_length),                                                                        \
    .std_dev = sys_cpu_to_be32(_std_dev),                                                                                    \
    .latitude = _latitude,                                                                     \
    .longitude = _longitude,                                                                   \
    .altitude = _altitude,                                                                     \
    .baseline_length = _baseline_length,                                                       \
    .attributes = 0}

struct px1122r_cmd_psti_interval_t {
    uint8_t msg_id;
    uint8_t msg_sub_id;
    uint8_t psti_id;
    uint8_t interval;
    uint8_t attributes;
} __attribute__((packed));

#define PX1122R_CONFIG_PSTI_MSG_INTERVAL(id, intv) \
    {.msg_id = 0x64, .msg_sub_id = 0x21, .psti_id = id, .interval = intv, .attributes = 0}

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

#define PX1122R_CONFIG_NMEA_TALKER_ID(id) {.msg_id = 0x4b, .talker_id = id, .attributes = 0}

struct px1122r_config_extended_msg_interval_t {
    uint8_t msg_id;
    uint8_t msg_sub_id;
    uint8_t gga_interval;
    uint8_t gsa_interval;
    uint8_t gsv_interval;
    uint8_t gll_interval;
    uint8_t rmc_interval;
    uint8_t vtg_interval;
    uint8_t zda_interval;
    uint8_t gns_interval;
    uint8_t gbs_interval;
    uint8_t grs_interval;
    uint8_t dtm_interval;
    uint8_t gst_interval;
    uint8_t attributes;
} __attribute__((packed));

#define PX1122R_CONFIG_EXTENDED_MSG_INTERVAL(gga, gsa, gsv, gll, rmc, vtg, zda, gns, gbs, grs, dtm, gst) \
    {.msg_id = 0x64, .msg_sub_id = 0x02,                                                                 \
    .gga_interval = gga,                                                                                 \
    .gsa_interval = gsa,                                                                                 \
    .gsv_interval = gsv,                                                                                 \
    .gll_interval = gll,                                                                                 \
    .rmc_interval = rmc,                                                                                 \
    .vtg_interval = vtg,                                                                                 \
    .zda_interval = zda,                                                                                 \
    .gns_interval = gns,                                                                                 \
    .gbs_interval = gbs,                                                                                 \
    .grs_interval = grs,                                                                                 \
    .dtm_interval = dtm,                                                                                 \
    .gst_interval = gst,                                                                                 \
    .attributes = 0                                                                                      \
    }
#endif
