#include <device.h>
#include <drivers/uart.h>
#include <errno.h>
#include <logging/log.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "transport.h"

LOG_MODULE_REGISTER(greybus_transport_uart, LOG_LEVEL_DBG);

/* Based on UniPro, from Linux */
#define CPORT_ID_MAX 4095

/* pad to not fill up ring buffer on GB_MTU */
#define RB_PAD 8
#define UART_RB_SIZE GB_MTU + RB_PAD

static int sendMessage(struct device *dev, struct gb_operation_hdr *msg);
static void uart_work_fn(struct k_work *work);

static struct device *uart_dev;

RING_BUF_DECLARE(uart_rb, UART_RB_SIZE);
static K_WORK_DEFINE(uart_work, uart_work_fn);

static void uart_work_fn(struct k_work *work)
{
	struct gb_operation_hdr *msg;
	uint8_t *data;
	size_t len;
	size_t msg_size;
	size_t payload_size;
	size_t expected_size;
	unsigned int cport = -1;
	int r;

	expected_size = sizeof(*msg);
	len = UART_RB_SIZE - ring_buf_space_get(&uart_rb);
	while(len < expected_size){
		len = UART_RB_SIZE - ring_buf_space_get(&uart_rb);
		k_usleep(100);
	}

	data = malloc(expected_size);
	len = ring_buf_get(&uart_rb, data, expected_size);
	msg = (struct gb_operation_hdr *)data;
	msg_size = sys_le16_to_cpu(msg->size);

	if (msg_size < sizeof(struct gb_operation_hdr)) {
		LOG_ERR("invalid message size %u", (unsigned)msg_size);
		free(data);
		return;
	}

	payload_size = msg_size - sizeof(struct gb_operation_hdr);
	if (payload_size > GB_MAX_PAYLOAD_SIZE) {
		LOG_ERR("invalid payload size %u", (unsigned)payload_size);
		free(data);
		return;
	}

	len = UART_RB_SIZE - ring_buf_space_get(&uart_rb);
	while(len < payload_size){
		len = UART_RB_SIZE - ring_buf_space_get(&uart_rb);
		k_usleep(100);
	}

	data = realloc(data, msg_size);
	len = ring_buf_get(&uart_rb, data + sizeof(struct gb_operation_hdr), payload_size);
	if (len != payload_size) {
		LOG_ERR("gb_operation payload not received");
		free(data);
		return;
	}

	LOG_HEXDUMP_DBG(msg, msg_size, "RX:");

	cport = sys_le16_to_cpu(*((uint16_t *)msg->pad));
	r = greybus_rx_handler(cport, msg, sys_le16_to_cpu(msg->size));
	if (r < 0) {
		LOG_DBG("failed to handle message : size: %u, id: %u, type: %u",
		sys_le16_to_cpu(msg->size), sys_le16_to_cpu(msg->id),
		msg->type);
	}
	free(data);
	return;
}

static int sendMessage(struct device *dev, struct gb_operation_hdr *msg)
{
	size_t offset = 0;
	size_t remaining;

	for (remaining = sys_le16_to_cpu(msg->size); remaining; remaining -= 1, offset += 1) {
		uart_poll_out(dev, ((uint8_t *)msg)[offset]);
	}

	return 0;
}

static void gb_xport_init(void)
{
}
static void gb_xport_exit(void)
{
}
static int gb_xport_listen(unsigned int cport)
{
	return 0;
}
static int gb_xport_stop_listening(unsigned int cport)
{
	return 0;
}
static int gb_xport_send(unsigned int cport, const void *buf, size_t len)
{
	int r;
	struct gb_operation_hdr *msg;

	msg = (struct gb_operation_hdr *)buf;
	if (NULL == msg) {
		LOG_ERR("message is NULL");
		return -EINVAL;
	}

	msg->pad[0] = cport;

	LOG_HEXDUMP_DBG(msg, sys_le16_to_cpu(msg->size), "TX:");

	if (sys_le16_to_cpu(msg->size) != len || len < sizeof(*msg)) {
		LOG_ERR("invalid message size %u (len: %u)",
			(unsigned)sys_le16_to_cpu(msg->size), (unsigned)len);
		return -EINVAL;
	}

	r = sendMessage(uart_dev, msg);

	return r;
}
static void *gb_xport_alloc_buf(size_t size)
{
	return malloc(size);
}
static void gb_xport_free_buf(void *ptr)
{
	free(ptr);
}

static const struct gb_transport_backend gb_xport = {
	.init = gb_xport_init,
	.exit = gb_xport_exit,
	.listen = gb_xport_listen,
	.stop_listening = gb_xport_stop_listening,
	.send = gb_xport_send,
	.send_async = NULL,
	.alloc_buf = gb_xport_alloc_buf,
	.free_buf = gb_xport_free_buf,
};

static void gb_xport_uart_isr(const struct device *dev, void *user_data)
{
	int r;
	uint8_t byte;
	uint8_t ovflw;
	size_t count;

	while (uart_irq_update(dev) &&
	       uart_irq_is_pending(dev)) {

		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		r = uart_fifo_read(dev, &byte, 1);
		if (r < 0) {
			LOG_ERR("uart_fifo_read() failed (%d)", r);
			uart_irq_rx_disable(dev);
			return;
		}

		if (0 == ring_buf_space_get(&uart_rb)) {
			r = ring_buf_get(&uart_rb, &ovflw, 1);
			if (r != 1) {
				LOG_ERR("failed to remove head of ring buffer");
				uart_irq_rx_disable(dev);
				return;
			}
			LOG_ERR("overflow occurred");
		}

		if (1 != ring_buf_put(&uart_rb, &byte, 1)) {
			LOG_ERR("ring_buf_put() failed");
			uart_irq_rx_disable(dev);
			return;
		}
	}
	count = UART_RB_SIZE - ring_buf_space_get(&uart_rb);
	if (count >= sizeof(struct gb_operation_hdr)) {
		k_work_submit(&uart_work);
	}
}

static int gb_xport_uart_init(void)
{
	int r;
	uint8_t c;

	LOG_INF("binding %s", CONFIG_GREYBUS_XPORT_UART_DEV);
	uart_dev = device_get_binding(CONFIG_GREYBUS_XPORT_UART_DEV);
	if (uart_dev == NULL) {
		LOG_ERR("unable to bind device named %s!", CONFIG_GREYBUS_XPORT_UART_DEV);
		r = -ENODEV;
		goto out;
	}
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);

	uart_irq_callback_set(uart_dev, gb_xport_uart_isr);

	/* Drain the fifo */
	while (uart_irq_rx_ready(uart_dev)) {
		uart_fifo_read(uart_dev, &c, 1);
	}

	uart_irq_rx_enable(uart_dev);

	r = 0;

out:
	return r;
}

struct gb_transport_backend *gb_transport_backend_init(unsigned int *cports, size_t num_cports) {

	int r;
	struct gb_transport_backend *ret = NULL;

	LOG_DBG("Greybus UART Transport initializing..");

	if (num_cports >= CPORT_ID_MAX) {
		LOG_ERR("invalid number of cports %u", (unsigned)num_cports);
		goto out;
		}

	r = gb_xport_uart_init();
	if (r < 0) {
		goto out;
	}

    ret = (struct gb_transport_backend *)&gb_xport;

	LOG_INF("Greybus UART Transport initialized");
	goto out;

out:
    return ret;
}
