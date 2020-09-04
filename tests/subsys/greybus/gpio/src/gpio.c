/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <errno.h>
#include <greybus/greybus.h>
#include <string.h>
#include <sys/util.h>
#include <ztest.h>
#include <zephyr.h>

#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) \
    || defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) \
    || defined(CONFIG_BOARD_NRF52_BSIM)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/byteorder.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

/* For some reason, including <net/net_ip.h> breaks everything
 * I only need these */
static inline struct sockaddr_in *net_sin(struct sockaddr *sa)
{
	return (struct sockaddr_in *)sa;
}

static inline struct sockaddr_in6 *net_sin6(struct sockaddr *sa)
{
	return (struct sockaddr_in6 *)sa;
}

#else

#include <net/net_ip.h>
#include <net/socket.h>
#include <posix/unistd.h>

#undef perror
#define perror(s) LOG_ERR("%s", s) 

#endif

/* slightly annoying */
#include "../../../../../subsys/greybus/gpio-gb.h"

#include "test-greybus-gpio.h"

#define TIMEOUT_MS 1000
#define PORT 4243

static struct device *gpio_dev;

static int fd = -1;

void test_greybus_setup(void) {

    struct sockaddr sa;
    socklen_t sa_len;
    int family;
    uint16_t *port;
    int r;

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		family = AF_INET6;
		net_sin6(&sa)->sin6_family = AF_INET6;
		net_sin6(&sa)->sin6_addr = in6addr_loopback;
		port = &net_sin6(&sa)->sin6_port;
		sa_len = sizeof(struct sockaddr_in6);
	} else if (IS_ENABLED(CONFIG_NET_IPV4)) {
		family = AF_INET;
		net_sin(&sa)->sin_family = AF_INET;
		net_sin(&sa)->sin_addr.s_addr = INADDR_ANY;
		port = &net_sin(&sa)->sin_port;
		sa_len = sizeof(struct sockaddr_in);
	} else {
		__ASSERT(false, "Neither IPv6 nor IPv4 is available");
	}

	*port = htons(PORT);

    greybus_service_init();

	gpio_dev = device_get_binding(GPIO_DEV_NAME);
	zassert_not_equal(gpio_dev, NULL, "failed to get device binding for " GPIO_DEV_NAME);

    r = socket(family, SOCK_STREAM, 0);
    __ASSERT(r >= 0, "connect: %d", errno);
    fd = r;

    r = connect(fd, &sa, sa_len);
    __ASSERT(r == 0, "connect: %d", errno);
}

void test_greybus_teardown(void) {
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

static void tx_rx(const struct gb_operation_hdr *req, struct gb_operation_hdr *rsp, size_t rsp_size)
{

    int r;
    int size;
    struct pollfd pollfd;
    const size_t hdr_size = sizeof(struct gb_operation_hdr);
    uint8_t null[1024];

    size = sys_le16_to_cpu(req->size);
    r = send(fd, req, size, 0);
    zassert_not_equal(r, -1, "send: %s", errno);
    zassert_equal(r, size, "write: expected: %d actual: %d", size, r);

    for(;;) {
		pollfd.fd = fd;
		pollfd.events = POLLIN;

		r = poll(&pollfd, 1, TIMEOUT_MS);
		zassert_not_equal(r, -1, "poll: %s", errno);
		zassert_not_equal(r, 0, "timeout waiting for response");
		zassert_equal(r, 1, "invalid number of pollfds with data: %d", r);

		r = recv(fd, rsp, hdr_size, 0);
		zassert_not_equal(r, -1, "recv: %s", errno);
		zassert_equal(hdr_size, r, "recv: expected: %u actual: %u", (unsigned)hdr_size, r);

		if (rsp->id != req->id || rsp->type != (GB_TYPE_RESPONSE_FLAG | req->type)) {

			r = recv(fd, null, sys_le16_to_cpu(rsp->size) - hdr_size, 0);
			zassert_not_equal(r, -1, "recv: %s", errno);
			zassert_equal(sys_le16_to_cpu(rsp->size) - hdr_size, r,
				"recv: expected: %u actual: %u",
				sys_le16_to_cpu(rsp->size) - hdr_size);
			/*
			 * Note: Currently, the greybus subsystem sends IRQ events
			 * kind of excessively, because there is no way to query
			 * the gpio subsystem for current pin configuration.
			 * Greybus irq events should naturally only be generated
			 * for input pins for example, and more specifically,
			 * input pins that are configured to generate interrupts!
			 * So we are effectively unable to filter events here until
			 * the ability to query gpio pin configuration is added to
			 * the Zephyr GPIO API.
			 *
			 * See https://github.com/zephyrproject-rtos/zephyr/issues/26938
			 */
			continue;
		}
		if (rsp_size - hdr_size > 0) {
			r = recv(fd, (uint8_t *)rsp + hdr_size, rsp_size - hdr_size, 0);
			zassert_not_equal(r, -1, "recv: %s", errno);
			zassert_equal(r, rsp_size - hdr_size, "expected: %u actual: %d", rsp_size - hdr_size, r);
		}

		zassert_equal(rsp->id, req->id, "expected: 0x%04x actual: 0x%04x", sys_le16_to_cpu(req->id), sys_le16_to_cpu(rsp->id));
		zassert_equal(rsp->type, GB_TYPE_RESPONSE_FLAG | req->type, "expected: %u actual: %u", GB_TYPE_RESPONSE_FLAG | req->type, rsp->type);
		break;
    }
}

void test_greybus_gpio_protocol_version(void) {
    const struct gb_operation_hdr req = {
        .size = sys_cpu_to_le16(sizeof(struct gb_operation_hdr)),
        .id = sys_cpu_to_le16(0xabcd),
        .type = GB_GPIO_TYPE_PROTOCOL_VERSION,
    };
    uint8_t rsp_[
        0
        + sizeof(struct gb_operation_hdr)
        + sizeof(struct gb_gpio_proto_version_response)
        ];
    const size_t rsp_size = sizeof(rsp_);
    struct gb_gpio_proto_version_response *const rsp =
        (struct gb_gpio_proto_version_response *)
        (rsp_ + sizeof(struct gb_operation_hdr));

    tx_rx(&req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);

    /* GB_GPIO_VERSION_MAJOR (0) is buried in subsys/greybus/gpio.c */
	zassert_equal(rsp->major, 0, "expected: %u actual: %u",
        0, rsp->major);

    /* GB_GPIO_VERSION_MINOR (1) is buried in subsys/greybus/gpio.c */
	zassert_equal(rsp->minor, 1, "expected: %u actual: %u",
        1, rsp->minor);
}

void test_greybus_gpio_cport_shutdown(void)
{
	/*
	 * looks like the original NuttX implementation was missing any kind
	 * of cport shutdown request
	 */
}

void test_greybus_gpio_line_count(void)
{
    const struct gb_operation_hdr req = {
        .size = sys_cpu_to_le16(sizeof(struct gb_operation_hdr)),
        .id = sys_cpu_to_le16(0xabcd),
        .type = GB_GPIO_TYPE_LINE_COUNT,
    };
    uint8_t rsp_[
        0
        + sizeof(struct gb_operation_hdr)
        + sizeof(struct gb_gpio_line_count_response)
        ];
    const size_t rsp_size = sizeof(rsp_);
    struct gb_gpio_line_count_response *const rsp =
        (struct gb_gpio_line_count_response *)
        (rsp_ + sizeof(struct gb_operation_hdr));
    size_t expected_count;
    size_t actual_count;

    tx_rx(&req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);

    zassert_not_equal(gpio_dev->config, NULL, "gpio_dev->config invalid");

    expected_count =
        popcount(((const struct gpio_driver_config *)gpio_dev->config)->port_pin_mask);
    actual_count = rsp->count + 1;

    zassert_equal(expected_count, actual_count, "expected: %u actual: %u",
        expected_count, actual_count);
}

void test_greybus_gpio_activate(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_activate_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_activate_request *const activate =
		(struct gb_gpio_activate_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_ACTIVATE;
    activate->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);
}

void test_greybus_gpio_deactivate(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_activate_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_activate_request *const activate =
		(struct gb_gpio_activate_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_DEACTIVATE;
    activate->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);
}

void test_greybus_gpio_get_direction(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_get_direction_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_get_direction_request *const dir_req =
		(struct gb_gpio_get_direction_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_GET_DIRECTION;
    dir_req->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_get_direction_response)
		 ];
    size_t rsp_size = sizeof(rsp_);
    struct gb_gpio_get_direction_response *const dir_rsp =
		(struct gb_gpio_get_direction_response *)
		(rsp_ + sizeof(struct gb_operation_hdr));

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);

    /* in greybus, 1 means input */
    zassert_equal(1, dir_rsp->direction, "expected: %u actual: %u", 1, dir_rsp->direction);
}

void test_greybus_gpio_direction_input(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_direction_in_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_direction_in_request *const dir_req =
		(struct gb_gpio_direction_in_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_DIRECTION_IN;
    dir_req->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);
}

void test_greybus_gpio_direction_output(void)
{
	int r;

    r = gpio_config(gpio_dev, GPIO_PIN_OUT, GPIO_INPUT);
    zassert_equal(0, r, "gpio_config() failed: %d", r);

    r = gpio_pin_get(gpio_dev, GPIO_PIN_IN);
    zassert_true(r >= 0, "gpio_pin_get() failed: %d", -r);
    zassert_equal(0, r, "expected: %u actual: %u", 0, r);

	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_direction_out_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_direction_out_request *const dir_req =
		(struct gb_gpio_direction_out_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_DIRECTION_OUT;
    dir_req->which = GPIO_PIN_OUT;
    dir_req->value = 1;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(GB_OP_SUCCESS, ((struct gb_operation_hdr *)rsp_)->result,
        "expected: %u actual: %u", GB_OP_SUCCESS,
        ((struct gb_operation_hdr *)rsp_)->result);

    r = gpio_pin_get(gpio_dev, GPIO_PIN_IN);
    zassert_true(r >= 0, "gpio_pin_get() failed: %d", -r);
    zassert_equal(1, r, "expected: %u actual: %u", 1, r);
}

void test_greybus_gpio_get_value(void)
{
	int r;
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_get_value_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_get_value_request *const get_req =
		(struct gb_gpio_get_value_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_GET_VALUE;
    get_req->which = GPIO_PIN_IN;

    /*
     * set PIN_OUT (1), since it is an output and is connected to PIN_IN, which
     * we will subsequently read
     */
    if (gpio_dev == NULL) {
        gpio_dev = device_get_binding(GPIO_DEV_NAME);
        zassert_not_equal(gpio_dev, NULL, "failed to get device binding for " GPIO_DEV_NAME);
    }
    r = gpio_pin_set(gpio_dev, GPIO_PIN_OUT, 1);
    zassert_equal(0, r, "gpio_pin_set() failed: %d", r);

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_get_value_response)
		 ];
    size_t rsp_size = sizeof(rsp_);
    struct gb_gpio_get_value_response *const get_rsp =
		(struct gb_gpio_get_value_response *)
		(rsp_ + sizeof(struct gb_operation_hdr));

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(GB_OP_SUCCESS, ((struct gb_operation_hdr *)rsp_)->result,
        "expected: %u actual: %u", GB_OP_SUCCESS,
        ((struct gb_operation_hdr *)rsp_)->result);

    /*
     * PIN_IN should be set (1), since it is an input and is
     * connected to PIN_OUT
     */
    zassert_equal(1, get_rsp->value, "expected: %u actual: %u", 1, get_rsp->value);
}

void test_greybus_gpio_set_value(void)
{
	int r;
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_set_value_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_set_value_request *const set_req =
		(struct gb_gpio_set_value_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_SET_VALUE;
    /* pin 1 is configured as output */
    set_req->which = GPIO_PIN_OUT;
    set_req->value = 1;

    /*
     * set PIN_OUT (1), since it is an output and is connected to PIN_IN, which
     * we will subsequently read
     */
    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(GB_OP_SUCCESS, ((struct gb_operation_hdr *)rsp_)->result,
        "expected: %u actual: %u", GB_OP_SUCCESS,
        ((struct gb_operation_hdr *)rsp_)->result);

    /*
     * PIN_IN should be set (1), since it is an input and is
     * connected to PIN_OUT
     */
    r = gpio_pin_get(gpio_dev, GPIO_PIN_IN);
    zassert_true(r >= 0, "gpio_pin_get() failed: %d", r);
    zassert_equal(1, r, "expected: 1 actual: %d", 1, r);
}

void test_greybus_gpio_set_debounce(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_set_debounce_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_set_debounce_request *const deb_req =
		(struct gb_gpio_set_debounce_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_SET_DEBOUNCE;
    deb_req->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    uint16_t usecs[] = {
		0, 11,
    };

    for(size_t i = 0; i < ARRAY_SIZE(usecs); ++i) {
		deb_req->usec = usecs[i];

		tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

		zassert_equal(GB_OP_SUCCESS, ((struct gb_operation_hdr *)rsp_)->result,
			"expected: %u: %u", GB_OP_SUCCESS,
			((struct gb_operation_hdr *)rsp_)->result);
    }
}

void test_greybus_gpio_irq_type(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_irq_type_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_irq_type_request *const irqt_req =
		(struct gb_gpio_irq_type_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    uint8_t irq_types[] = {
		GB_GPIO_IRQ_TYPE_NONE,
		GB_GPIO_IRQ_TYPE_EDGE_RISING,
		GB_GPIO_IRQ_TYPE_EDGE_FALLING,
		GB_GPIO_IRQ_TYPE_EDGE_BOTH,
		GB_GPIO_IRQ_TYPE_LEVEL_HIGH,
		GB_GPIO_IRQ_TYPE_LEVEL_LOW,
    };

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_IRQ_TYPE;
    irqt_req->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    for(size_t i = 0; i < ARRAY_SIZE(irq_types); ++i) {
		irqt_req->type = irq_types[i];

		tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

		zassert_equal(GB_OP_SUCCESS, ((struct gb_operation_hdr *)rsp_)->result,
			"expected: %u actual: %u", GB_OP_SUCCESS,
			((struct gb_operation_hdr *)rsp_)->result);
    }
}

void test_greybus_gpio_irq_mask(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_irq_mask_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_irq_mask_request *const dir_req =
		(struct gb_gpio_irq_mask_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_IRQ_MASK;
    dir_req->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);
}

void test_greybus_gpio_irq_unmask(void)
{
	uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_irq_unmask_request)
		 ] = {};
    struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_irq_unmask_request *const dir_req =
		(struct gb_gpio_irq_unmask_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    req->size = sys_cpu_to_le16(sizeof(req_));
    req->id = sys_cpu_to_le16(0xabcd);
    req->type = GB_GPIO_TYPE_IRQ_UNMASK;
    dir_req->which = GPIO_PIN_IN;

    uint8_t rsp_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 ];
    size_t rsp_size = sizeof(rsp_);

    tx_rx(req, (struct gb_operation_hdr *)rsp_, rsp_size);

    zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
        "expected: GB_OP_SUCCESS actual: %u",
        ((struct gb_operation_hdr *)rsp_)->result);
}

void test_greybus_gpio_irq_event(void)
{
    int r;
    struct pollfd pollfd = {};
    uint8_t req_[
		 0
		 + sizeof(struct gb_operation_hdr)
		 + sizeof(struct gb_gpio_irq_event_request)
		 ] = {};
    const size_t req_size = sizeof(req_);
	struct gb_operation_hdr *const req = (struct gb_operation_hdr *)req_;
    struct gb_gpio_irq_event_request *const irq_event_req =
		(struct gb_gpio_irq_event_request *)
		(req_ + sizeof(struct gb_operation_hdr));

    r = gpio_pin_configure(gpio_dev, GPIO_PIN_IN, GPIO_INPUT);
    zassert_equal(0, r, "gpio_pin_interrupt_configure() failed: %d", r);

    r = gpio_pin_interrupt_configure(gpio_dev, GPIO_PIN_IN, GPIO_INT_EDGE_RISING);
    zassert_equal(0, r, "gpio_pin_interrupt_configure() failed: %d", r);

    r = gpio_pin_configure(gpio_dev, GPIO_PIN_OUT, GPIO_OUTPUT);
    zassert_equal(0, r, "gpio_pin_configure() failed: %d", r);

    r = gpio_pin_set(gpio_dev, GPIO_PIN_OUT, 0);
    zassert_equal(0, r, "gpio_pin_set() failed: %d", r);

    r = gpio_pin_set(gpio_dev, GPIO_PIN_OUT, 1);
    zassert_equal(0, r, "gpio_pin_set() failed: %d", r);

    for(;;) {
    	memset(&pollfd, 0, sizeof(pollfd));
		pollfd.fd = fd;
		pollfd.events = POLLIN;

		r = poll(&pollfd, 1, TIMEOUT_MS);
		zassert_not_equal(r, -1, "poll: %d", errno);
		zassert_not_equal(r, 0, "timeout waiting for response");
		zassert_equal(r, 1, "invalid number of pollfds with data: %d", r);

		r = recv(fd, req, req_size, 0);
		zassert_not_equal(r, -1, "read: %d", errno);
		zassert_equal(r, req_size, "expected: %u actual: %d", req_size, r);

		zassert_equal(GB_GPIO_TYPE_IRQ_EVENT, req->type, "expected: %u actual: %u", GB_GPIO_TYPE_IRQ_EVENT, req->type);

		/* Again... cannot query Zephyr GPIO pin configuration, and therefore cannot filter */
		/* zassert_equal(GPIO_PIN_IN, irq_event_req->which, "expected: %u actual: %u", GPIO_PIN_IN, irq_event_req->which); */

		if (GPIO_PIN_IN == irq_event_req->which) {
			break;
		}
    }
}
