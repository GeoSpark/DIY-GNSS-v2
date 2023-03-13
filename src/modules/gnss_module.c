#include "px1122r.h"
// #include "events/gnss_event.h"

#define MODULE gnss
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);
static const struct device* dev = DEVICE_DT_GET(DT_INST(0, skytraq_px1122r));
static bool is_streaming = false;
uint8_t output_buf[128];

static void stream_cb(const uint8_t* buf, const uint8_t len) {
//    static uint8_t* buf_ptr = output_buf;
//
//    for (int i = 0; i < len; ++i) {
//        *buf_ptr++ = buf[i];
//
//        if (buf[i] == 0x0a) {
//            LOG_DBG("%.*s", (buf_ptr - output_buf) - 2, output_buf);
//            buf_ptr = output_buf;
//        }
//    }
}

static bool handle_button_event(const struct button_event* event) {
    if (event->pressed) {
        if (!is_streaming) {
            struct px1122r_cmd_rtk_mode_t msg;
            msg.msg_id = 0x6a;
            msg.msg_sub_id = 0x06;
            msg.mode = 0;
            msg.function = 0;
            msg.survey_length = sys_be16_to_cpu(60);
            msg.std_dev = sys_be16_to_cpu(3);
            msg.latitude = 0.0;
            msg.longitude = 0.0;
            // An example of doing some bit-twiddling magic.
            float alt = 0.0f;
            msg.altitude = sys_be32_to_cpu(*(uint32_t*)&alt);
            msg.baseline_length = 0.0f;
            msg.attributes = 0;
            px1122r_send_command(dev, &msg, sizeof(msg));
            // The command above resets the PX1122R, and it takes it a little while to start receiving commands again.
            k_msleep(30);

            struct px1122r_cmd_psti_interval_t psti_interval;
            // Disable STI,030 – Recommended Minimum 3D GNSS Data
            psti_interval.msg_id = 0x64;
            psti_interval.msg_sub_id = 0x21;
            psti_interval.psti_id = 30;
            psti_interval.interval = 0;
            psti_interval.attributes = 0;
            px1122r_send_command(dev, &psti_interval, sizeof(psti_interval));

            // Disable STI,033 – RTK RAW Measurement Monitoring Data
            psti_interval.psti_id = 33;
            psti_interval.interval = 0;
            px1122r_send_command(dev, &psti_interval, sizeof(psti_interval));

            struct px1122r_cmd_nmea_string_interval_t nmea_interval;
            nmea_interval.msg_id = 0x64;
            nmea_interval.msg_sub_id = 0x3b;
            nmea_interval.attributes = 0;
            nmea_interval.nmea_id[0] = 'G';
            nmea_interval.nmea_id[1] = 'S';
            nmea_interval.nmea_id[2] = 'T';
            nmea_interval.interval = 1;
            px1122r_send_command(dev, &nmea_interval, sizeof(nmea_interval));

            struct px1122r_cmd_nmea_talker_id_t nmea_talker_id;
            nmea_talker_id.msg_id = 0x4b;
            nmea_talker_id.talker_id = TALKER_ID_GN_MODE;
            nmea_talker_id.attributes = 0;
            if (px1122r_send_command(dev, &nmea_talker_id, sizeof(nmea_talker_id)) == 0) {
                px1122r_start_stream(dev, stream_cb);
            }
        } else {
            px1122r_stop_stream(dev);
        }

        is_streaming = !is_streaming;
//        px1122r_send_command(dev);
    }

    return false;
}

static void init_fn(void) {
	if (!device_is_ready(dev)) {
		LOG_ERR("PX1122R not ready.");
		module_set_state(MODULE_STATE_ERROR);
		return;
	}

	module_set_state(MODULE_STATE_READY);
}

static bool app_event_handler(const struct app_event_header *aeh) {
	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			init_fn();

			return false;
		}

		return false;
	}
    
    if (is_button_event(aeh)) {
        return handle_button_event(cast_button_event(aeh));
    }

    return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
// APP_EVENT_SUBSCRIBE(MODULE, gnss_event);
APP_EVENT_SUBSCRIBE(MODULE, button_event);
