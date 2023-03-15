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
#define BUF_SIZE 256
#define MAX_PAYLOAD_SIZE 128
#define RX_BUF_TIMEOUT_US 250

struct px1122r_dev_data {
    const struct device* uart_dev;
    const struct device* uart_dev2;
    uint8_t cur_buf;
    uint8_t rx_bufs[NUM_RX_BUFS][BUF_SIZE];
    uint8_t tx_buf[BUF_SIZE];
    bool stream_mode;
    px1122r_callback_t callback;
    int8_t command_response;
};

#define WORKER_STACK_SIZE 512
#define WORKER_PRIORITY 5

K_THREAD_STACK_DEFINE(worker_stack_area, WORKER_STACK_SIZE);
static K_SEM_DEFINE(command_sem, 0, 1);

struct k_work_q work_queue;

struct work_item_t {
    struct k_work work;
    struct px1122r_dev_data* dev_data;
    uint8_t* buf;
    uint16_t len;
} work_item;

struct skytraq_message_t {
    uint16_t payload_length;
    uint8_t message_type;
    uint8_t payload[0];
};

static uint8_t calc_checksum(const uint8_t* msg, uint16_t length) {
    uint8_t cs = 0;

    for (uint16_t i = 0; i < length; ++i) {
        cs ^= *msg++;
    }

    return cs;
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

    // We're casting away constness for the user-data, but we'll be fine as long as we cast it back in the handler.
    int err = uart_callback_set(data->uart_dev, uart_cb, (void*)dev);

    if (err != 0) {
        LOG_ERR("Failed to init UART callback");
        return -EINVAL;
    }

    LOG_DBG("PX1122R initialized");

    return 0;
}

static int8_t display_skytraq_message(const uint8_t* buffer) {
    const struct skytraq_message_t* message = (const struct skytraq_message_t*)buffer;

    switch (message->message_type) {
        case 0x83:
            LOG_DBG("ACK 0x%02x", message->payload[0]);
            return 1;

        case 0x84:
            LOG_DBG("NACK 0x%02x", message->payload[0]);
            return 0;

        default:
            LOG_WRN("Unknown response 0x%02x", message->message_type);
//            LOG_HEXDUMP_DBG(message->payload, sys_be16_to_cpu(message->payload_length) - 1, "Payload");
            return -1;
    }
}

static bool handle_skytraq_message(uint8_t* buf, uint8_t buf_len, struct px1122r_dev_data* data) {
    static int state = 0;
    static uint16_t payload_len = 0;
    static uint16_t payload_counter = 0;
    static uint8_t checksum = 0;
    static uint8_t payload_buf[MAX_PAYLOAD_SIZE];
    uint8_t* current = buf;
    uint8_t* current_payload = payload_buf;
    bool ret = false;

    for (uint32_t i = 0; i < buf_len; ++i, current++) {
        switch (state) {
            case 0:
                checksum = 0;
                payload_len = 0;
                payload_counter = 0;
                current_payload = payload_buf;
                ret = false;
                if (*current == 0xa0) {
                    state = 1;
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
                *(current_payload++) = *current;
                state = 3;
                break;
            case 3:
                payload_len |= *current;
                if (payload_len > MAX_PAYLOAD_SIZE) {
                    LOG_WRN("Payload size too big for buffer %d. Skipping packet.", payload_len);
                    state = 0;
                    break;
                }
                *(current_payload++) = *current;
                payload_counter = payload_len;
                state = 4;
                break;
            case 4:
                --payload_counter;
                checksum ^= *current;
                *(current_payload++) = *current;

                if (payload_counter == 0) {
                    state = 5;
                }
                break;
            case 5:
                if (checksum != *current) {
                    LOG_WRN("Checksum mismatch calc %x got %x", checksum, *current);
                }
                state = 6;
                break;
            case 6:
                // Postamble 1
                state = 7;
                break;
            case 7:
                // Postamble 2
                state = 0;
                ret = true;
                data->command_response = display_skytraq_message(payload_buf);
                k_sem_give(&command_sem);
                break;
            default:
                LOG_WRN("Unknown state %d", state);
                break;
        }
    }

    return ret;
}

static void work_handler(struct k_work* work) {
    struct work_item_t* my_work = (struct work_item_t*) work;
    struct px1122r_dev_data* data = my_work->dev_data;
    static int state = 0;

    if (data->stream_mode) {
        if (data->callback != NULL) {
            data->callback(my_work->buf, my_work->len);
        }
    } else {
        if (state == 0) {
            for (int i = 0; i < my_work->len; ++i) {
                if (my_work->buf[i] == 0xa0) {
                    if (!handle_skytraq_message(&my_work->buf[i], my_work->len - i, data)) {
                        state = 1;
                    }
                    break;
                }
            }
        } else {
            if (handle_skytraq_message(&my_work->buf[0], my_work->len, data)) {
                // TODO: We might have data left in the buffer. Do something with it.
                // I think it's only a problem if a message ends at the end of the buffer.
                state = 0;
            }
        }
    }
}

static void uart_cb(const struct device* dev, struct uart_event* evt, void* user_data) {
    const struct device* px1122r_dev = user_data;
    struct px1122r_dev_data* data = px1122r_dev->data;

    switch (evt->type) {
        case UART_RX_RDY:
            if (!k_work_is_pending(&work_item.work)) {
                work_item.dev_data = data;
                work_item.buf = evt->data.rx.buf + evt->data.rx.offset;
                LOG_DBG("len %d", evt->data.rx.len);
                work_item.len = evt->data.rx.len;
                k_work_submit_to_queue(&work_queue, &work_item.work);
            } else {
                LOG_WRN("Work item still pending");
            }
            break;

        case UART_RX_BUF_REQUEST:
            data->cur_buf = (data->cur_buf + 1) % NUM_RX_BUFS;
            uart_rx_buf_rsp(dev, data->rx_bufs[data->cur_buf], BUF_SIZE);
            break;

        default:
            break;
    }
}

int px1122r_start_stream(const struct device* dev, px1122r_callback_t cb) {
    struct px1122r_dev_data* data = dev->data;
    data->callback = cb;
    data->stream_mode = true;
    uart_rx_enable(data->uart_dev, data->rx_bufs[data->cur_buf], BUF_SIZE, RX_BUF_TIMEOUT_US);
    return 0;
}

int px1122r_stop_stream(const struct device* dev) {
    struct px1122r_dev_data* data = dev->data;
    data->stream_mode = false;
    uart_rx_disable(data->uart_dev);

    return 0;
}

int px1122r_send_command(const struct device* dev, const void* command, const uint16_t length) {
    struct px1122r_dev_data* data = dev->data;

    data->stream_mode = false;
    uart_rx_enable(data->uart_dev, data->rx_bufs[data->cur_buf], BUF_SIZE, RX_BUF_TIMEOUT_US);

    data->tx_buf[0] = 0xa0;
    data->tx_buf[1] = 0xa1;
    data->tx_buf[2] = length >> 8;
    data->tx_buf[3] = length & 0xff;
    memcpy(&data->tx_buf[4], command, length);
    data->tx_buf[length + 4] = calc_checksum(command, length);
    data->tx_buf[length + 5] = 0x0d;
    data->tx_buf[length + 6] = 0x0a;

    uart_tx(data->uart_dev, data->tx_buf, length + 7, SYS_FOREVER_US);

    int sem_ret = k_sem_take(&command_sem, K_MSEC(100));
    if (sem_ret != 0) {
        LOG_ERR("Failed to send command 0x%02x. %d", data->tx_buf[4], sem_ret);
        uart_rx_disable(data->uart_dev);
        return -1;
    }

    uart_rx_disable(data->uart_dev);

    if (!data->command_response) {
        LOG_ERR("NACK received");
        return -1;
    }

    return 0;
}

#define PX1122R_DEFINE(inst)                                               \
    static struct px1122r_dev_data px1122r_data_##inst = {                 \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(0)),                         \
        .uart_dev2 = DEVICE_DT_GET(DT_NODELABEL(uart1)),                   \
        .stream_mode = false,                                              \
        .cur_buf = 0,                                                      \
        .callback = NULL,                                                  \
        .command_response = false,                                         \
    };                                                                     \
    DEVICE_DT_INST_DEFINE(inst,                                            \
                          px1122r_init,                                    \
                          NULL,                                            \
                          &px1122r_data_##inst,                            \
                          NULL,                                            \
                          POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,   \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(PX1122R_DEFINE)
