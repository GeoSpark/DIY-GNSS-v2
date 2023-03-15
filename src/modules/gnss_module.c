#include "px1122r.h"
#include "events/data_event.h"
#include "events/gnss_event.h"

#define MODULE gnss
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);
static const struct device* dev = DEVICE_DT_GET(DT_INST(0, skytraq_px1122r));
static bool is_streaming = false;

#define BUF_SIZE 512
#define NUM_BUFS 2

static uint8_t bufs[NUM_BUFS][BUF_SIZE];
static uint8_t cur_buf = 0;

#define WORKER_STACK_SIZE 512
#define WORKER_PRIORITY 6

K_THREAD_STACK_DEFINE(gnss_worker_stack_area, WORKER_STACK_SIZE);

static struct k_work_q gnss_work_queue;
static struct work_item_t {
    struct k_work work;
    struct config_t config;
    enum data_event_type event_type;
} gnss_work_item;

static void stream_cb(const uint8_t* buf, uint16_t len) {
    // TODO: Something clever with incoming data being bigger than our buffer.
    if (len > BUF_SIZE) {
        LOG_WRN("Trying to put %d bytes into a %d byte buffer", len, BUF_SIZE);
        len = BUF_SIZE;
    }

    memcpy(bufs[cur_buf], buf, len);

    struct gnss_event* event = new_gnss_event();
    event->bytes = bufs[cur_buf];
    event->size = len;
    APP_EVENT_SUBMIT(event);
    cur_buf = (cur_buf + 1) % NUM_BUFS;
}

static bool handle_button_event(const struct button_event* event) {
    if (event->pressed) {
        if (!is_streaming) {
            px1122r_start_stream(dev, stream_cb);
        } else {
            px1122r_stop_stream(dev);
        }

        is_streaming = !is_streaming;
    }

    return false;
}

static bool handle_data_event(const struct data_event* event) {
    gnss_work_item.config = event->config;
    gnss_work_item.event_type = event->event_type;
    k_work_submit_to_queue(&gnss_work_queue, &gnss_work_item.work);

    return false;
}

static void work_handler(struct k_work* work) {
    struct work_item_t* my_work = (struct work_item_t*) work;

    uint8_t i = my_work->config.sample_interval;
    LOG_DBG("Setting the interval to %d", i);

    if (my_work->event_type == DATA_EVENT_CONFIG_INITIAL) {
        struct px1122r_cmd_psti_interval_t psti_interval = PX1122R_CONFIG_PSTI_MSG_INTERVAL(30, 0);
        px1122r_send_command(dev, &psti_interval, sizeof(psti_interval));

        struct px1122r_cmd_psti_interval_t psti_interval32 = PX1122R_CONFIG_PSTI_MSG_INTERVAL(32, 0);
        px1122r_send_command(dev, &psti_interval32, sizeof(psti_interval32));

        struct px1122r_cmd_psti_interval_t psti_interval33 = PX1122R_CONFIG_PSTI_MSG_INTERVAL(33, 0);
        px1122r_send_command(dev, &psti_interval33, sizeof(psti_interval33));

        struct px1122r_cmd_nmea_talker_id_t talker_id = PX1122R_CONFIG_NMEA_TALKER_ID(TALKER_ID_GN_MODE);
        px1122r_send_command(dev, &talker_id, sizeof(talker_id));
    }

    struct px1122r_config_extended_msg_interval_t nmea_interval =
            PX1122R_CONFIG_EXTENDED_MSG_INTERVAL(i, i, i, 0, i, i, i, 0, 0, 0, 0, i);
    px1122r_send_command(dev, &nmea_interval, sizeof(nmea_interval));
    // The command above resets the PX1122R, and it takes it a little while to start receiving commands again.
    k_msleep(30);

    double lat = 0.0;
    double lng = 0.0;
    float alt = 0.0f;
    float bll = 0.0f;
    struct px1122r_cmd_rtk_mode_t msg = PX1122R_CMD_RTK_MODE(0, 0, 60, 3,
            sys_cpu_to_be64(*(uint64_t*)&lat), sys_cpu_to_be64(*(uint64_t*)&lng),
            sys_cpu_to_be32(*(uint32_t*)&alt),
            sys_cpu_to_be32(*(uint32_t*)&bll));
    px1122r_send_command(dev, &msg, sizeof(msg));
}

static void init_fn(void) {
	if (!device_is_ready(dev)) {
		LOG_ERR("PX1122R not ready.");
		module_set_state(MODULE_STATE_ERROR);
		return;
	}

    k_work_queue_init(&gnss_work_queue);
    k_work_init(&gnss_work_item.work, work_handler);
    k_work_queue_start(&gnss_work_queue, gnss_worker_stack_area,
                       K_THREAD_STACK_SIZEOF(gnss_worker_stack_area), WORKER_PRIORITY,
                       NULL);

    module_set_state(MODULE_STATE_READY);
}

static bool app_event_handler(const struct app_event_header *aeh) {
	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			init_fn();
			return false;
		}

		return false;
	}
    
    if (is_button_event(aeh)) {
        return handle_button_event(cast_button_event(aeh));
    }

    if (is_data_event(aeh)) {
        return handle_data_event(cast_data_event(aeh));
    }

    return false;
}


APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, data_event);
APP_EVENT_SUBSCRIBE(MODULE, button_event);
