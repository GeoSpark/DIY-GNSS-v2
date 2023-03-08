#include "px1122r.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#define LOG_LEVEL CONFIG_PX1122R_LOG_LEVEL
#define DT_DRV_COMPAT skytraq_px1122r

LOG_MODULE_REGISTER(PX1122R, LOG_LEVEL);

static void uart_cb(const struct device* dev, struct uart_event* evt, void* user_data);

static void work_handler(struct k_work* work);

#define NUM_RX_BUFS 2
#define BUF_SIZE 64
#define MAX_PAYLOAD_SIZE 81
uint8_t rx_bufs[NUM_RX_BUFS][BUF_SIZE];

struct px1122r_dev_data {
    const struct device* uart_dev;
    const struct device* uart_dev2;
    uint8_t cur_buf;
    bool stream_mode;
};

struct px1122r_dev_cfg {
};

#define WORKER_STACK_SIZE 512
#define WORKER_PRIORITY 5

K_THREAD_STACK_DEFINE(worker_stack_area, WORKER_STACK_SIZE);

struct k_work_q work_queue;

struct work_item_t {
    struct k_work work;
    uint8_t* buf;
    uint8_t len;
} work_item;

struct skytraq_message_t {
    uint16_t preamble;
    uint16_t payload_length;
    uint8_t payload[0];
};

#define MSG_PREAMBLE 0xa0, 0xa1
#define MSG_POSTAMBLE 0x0d, 0x0a
#define UINT8_PARAM(val) (val)
#define UINT16_PARAM(val) (val & 0xff), (val >> 8)

#define MSG_20(p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) \
    {MSG_PREAMBLE, 0x00, 0x11, 0x20,                                                 \
    UINT8_PARAM(p2), UINT8_PARAM(p3), UINT8_PARAM(p4), UINT8_PARAM(p5), UINT8_PARAM(p6), UINT8_PARAM(p7), \
    UINT8_PARAM(p8), UINT8_PARAM(p9), UINT8_PARAM(p10), UINT8_PARAM(p11), UINT8_PARAM(p12), UINT8_PARAM(p13), \
    UINT8_PARAM(p14), UINT8_PARAM(p15), UINT8_PARAM(p16), UINT8_PARAM(p17),            \
    0x00, MSG_POSTAMBLE}

static void calc_checksum(struct skytraq_message_t* msg) {
    uint8_t cs = 0;
    uint8_t* payload = msg->payload;

    for (uint16_t i = 0; i < sys_be16_to_cpu(msg->payload_length); ++i) {
        cs ^= *payload++;
    }

    *payload = cs;
}

static int px1122r_init(const struct device* dev) {
    struct px1122r_dev_data* data = dev->data;

    if (!device_is_ready(data->uart_dev) || !device_is_ready(data->uart_dev2)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    k_work_queue_init(&work_queue);
    k_work_init(&work_item.work, work_handler);
    k_work_queue_start(&work_queue, worker_stack_area,
                       K_THREAD_STACK_SIZEOF(worker_stack_area), WORKER_PRIORITY,
                       NULL);

    int err = uart_callback_set(data->uart_dev, uart_cb, NULL);

    if (err != 0) {
        LOG_ERR("Failed to init UART callback");
        return -EINVAL;
    }

    return 0;
}

static bool handle_skytraq_message(uint8_t* buf, uint8_t buf_len) {
    static int state = 0;
    static uint16_t payload_len = 0;
    static uint16_t payload_counter = 0;
    static uint8_t cs = 0;
    static uint8_t payload_buf[MAX_PAYLOAD_SIZE];
    uint8_t* current = buf;
    uint8_t* current_payload = payload_buf;
    bool ret = false;

    for (uint32_t i = 0; i < buf_len; ++i, current++) {
        switch (state) {
            case 0:
                cs = 0;
                payload_len = 0;
                payload_counter = 0;
                current_payload = payload_buf;
                ret = false;
                if (*current == 0xa0) {
                    state = 1;
                } else {
                    // Hand this buffer off to another function and leave.
                    state = 0;
                    LOG_DBG("Unknown stuff here %x", *current);
                    return true;
                }
                break;
            case 1:
                if (*current == 0xa1) {
                    state = 2;
                } else {
                    state = 0;
                }
                break;
            case 2:
                payload_len = *current << 8;
                state = 3;
                break;
            case 3:
                payload_len |= *current;
                if (payload_len > MAX_PAYLOAD_SIZE) {
                    LOG_WRN("Payload size too big for buffer %d. Truncating", payload_len);
                    payload_len = MAX_PAYLOAD_SIZE;
                }
                payload_counter = payload_len;
                state = 4;
                break;
            case 4:
                --payload_counter;
                cs ^= *current;
                *(current_payload++) = *current;
                LOG_INF("Message type %x", *current);

                if (payload_counter == 0) {
                    state = 6;
                } else {
                    state = 5;
                }
                break;

            case 5:
                --payload_counter;
                cs ^= *current;
                *(current_payload++) = *current;

                if (payload_counter == 0) {
                    state = 6;
                }
                break;
            case 6:
                if (cs != *current) {
                    LOG_WRN("Checksum mismatch calc %x got %x", cs, *current);
                } else {
                    LOG_DBG("Checksum OK");
                }
                // TODO: Something with the data here.
                state = 7;
                break;
            case 7:
                // Postamble 1
                state = 8;
                break;
            case 8:
                // Postamble 2
                state = 0;
                ret = true;
                break;
            default:
                LOG_WRN("Unknown state %d", state);
                break;
        }
    }

    return ret;
}

static uint16_t handle_rtcm_message(uint8_t* buf, uint8_t buf_len) {
    static int state = 0;
    static uint16_t payload_len = 0;
    static uint16_t payload_counter = 0;
    static uint8_t buf_idx = 0;
    static uint8_t payload_buf[BUF_SIZE];
    static uint32_t checksum = 0;
    uint8_t* current = buf;
    bool finished = false;
    uint16_t total_read = 0;

    for (uint32_t i = 0; i < buf_len; ++i, current++) {
        switch (state) {
            case 0:
                payload_len = 0;
                payload_counter = 0;
                checksum = 0;
                buf_idx = 0;
                finished = false;
                if (*current == 0xd3) {
                    state = 1;
                } else {
                    // Hand this buffer off to another function and leave.
                    finished = true;
                }
                break;
            case 1:
                payload_len = *current << 8;
                state = 2;
                break;
            case 2:
                payload_len |= *current;
                state = 3;
                LOG_DBG("RTCM message length %d", payload_len);
                break;
            case 3:
                // TODO: We can do a bulk copy here I reckon.
                ++payload_counter;

                if (payload_counter == payload_len) {
                    state = 4;
                }
                break;
            case 4:
                checksum = *current << 16;
                state = 5;
                break;
            case 5:
                checksum |= *current << 8;
                state = 6;
                break;
            case 6:
                checksum |= *current;
                state = 0;
                finished = true;
                LOG_DBG("RTCM message checksum %x", checksum);
                break;
        }

        payload_buf[buf_idx++] = *current;
        if (buf_idx == BUF_SIZE) {
            // TODO: Dispatch buffer.
            LOG_DBG("Dispatching %d bytes", buf_idx);
            buf_idx = 0;
        }

        total_read = i;

        if (finished) {
            break;
        }
    }

    return total_read;
}

static bool handle_nmea_message(uint8_t* buf, uint8_t buf_len) {
    return true;
}

static void work_handler(struct k_work* work) {
    struct work_item_t* my_work = (struct work_item_t*) work;
    static int state = 0;

    switch (state) {
        case 0:
            if (my_work->buf[0] == 0xa0) {
                if (handle_skytraq_message(my_work->buf, my_work->len)) {
                    state = 0;
                } else {
                    state = 1;
                }
            } else if (my_work->buf[0] == 0xd3) {
                if (handle_rtcm_message(my_work->buf, my_work->len) < my_work->len) {
                    state = 0;
                } else {
                    state = 2;
                }
            } else {
                LOG_HEXDUMP_DBG(my_work->buf, my_work->len, "Unknown data");
            }
            break;
        case 1:
            if (handle_skytraq_message(my_work->buf, my_work->len)) {
                state = 0;
            }
            break;
        case 2:
            if (handle_rtcm_message(my_work->buf, my_work->len)) {
                state = 0;
            }
            break;
        default:
            state = 0;
            break;
    }

}

static void uart_cb(const struct device* dev, struct uart_event* evt, void* user_data) {
    ARG_UNUSED(user_data);

    struct px1122r_dev_data* data = dev->data;

    switch (evt->type) {
        case UART_TX_DONE:
            break;

        case UART_TX_ABORTED:
            break;

        case UART_RX_RDY:
            if (!k_work_is_pending(&work_item.work)) {
                work_item.buf = evt->data.rx.buf + evt->data.rx.offset;
                work_item.len = evt->data.rx.len;
                k_work_submit_to_queue(&work_queue, &work_item.work);
            } else {
                LOG_WRN("Work item still pending");
            }
//            if (!data->stream_mode) {
//                int err = uart_rx_disable(data->uart_dev);
//                if (err != 0) {
//                    LOG_ERR("Failed to stop UART rx");
//                }
//            }
            break;

        case UART_RX_BUF_REQUEST:
            data->cur_buf = (data->cur_buf + 1) % NUM_RX_BUFS;
            uart_rx_buf_rsp(dev, rx_bufs[data->cur_buf], BUF_SIZE);
            break;

        case UART_RX_BUF_RELEASED:
            break;

        case UART_RX_DISABLED:
            break;

        case UART_RX_STOPPED:
            break;
    }
}

//static const uint8_t msg[] = {0xa0, 0xa1, 0x00, 0x02, 0x02, 0x00, 0x02, 0x0d, 0x0a};

int px1122r_start_stream(const struct device* dev) {
    struct px1122r_dev_data* data = dev->data;

    int err = uart_rx_enable(data->uart_dev, rx_bufs[data->cur_buf], BUF_SIZE, 100);
    if (err != 0) {
        LOG_ERR("Failed to start UART rx");
        return err;
    }

    data->stream_mode = true;

    return 0;
}

int px1122r_stop_stream(const struct device* dev) {
    struct px1122r_dev_data* data = dev->data;

    data->stream_mode = false;

    int err = uart_rx_disable(data->uart_dev);
    if (err != 0) {
        LOG_ERR("Failed to stop UART rx");
        return err;
    }

    return 0;
}

int px1122r_send_command(const struct device* dev) {
    struct px1122r_dev_data* data = dev->data;

    data->stream_mode = true;
//    const uint8_t version_msg[] = {0xa0, 0xa1, 0x00, 0x02, 0x02, 0x00, 0x02, 0x0d, 0x0a};
    uint8_t no_output_msg[] = MSG_20(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0);
    calc_checksum((struct skytraq_message_t*)no_output_msg);

    int err = uart_rx_enable(data->uart_dev, rx_bufs[data->cur_buf], BUF_SIZE, 100);
    if (err != 0) {
        LOG_ERR("Failed to start UART rx");
    }

    err = uart_tx(data->uart_dev, no_output_msg, sizeof(no_output_msg), SYS_FOREVER_US);

    if (err != 0) {
        LOG_ERR("Failed to send command");
        return err;
    }

    return 0;
}

#define PX1122R_DEFINE(inst)                                               \
    static struct px1122r_dev_data px1122r_data_##inst = {                 \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(0)),                         \
        .uart_dev2 = DEVICE_DT_GET(DT_NODELABEL(uart1)),                   \
        .stream_mode = false,                                              \
        .cur_buf = 0,                                                      \
    };                                                                     \
    static const struct px1122r_dev_cfg px1122r_cfg_##inst = {             \
    };                                                                     \
    DEVICE_DT_INST_DEFINE(inst,                                            \
                          px1122r_init,                                    \
                          NULL,                                            \
                          &px1122r_data_##inst,                            \
                          &px1122r_cfg_##inst,                             \
                          POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,   \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(PX1122R_DEFINE)
