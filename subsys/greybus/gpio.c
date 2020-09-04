/*
 * Copyright (c) 2014-2015 Google Inc.
 * Copyright (c) 2020, Friedt Professional Engineering Services, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 * Author: Christopher Friedt <chrisfriedt@gmail.com>
 */

#include <logging/log.h>
#include <drivers/gpio.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <sys/byteorder.h>

#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) \
	|| defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) \
	|| defined(CONFIG_BOARD_NRF52_BSIM)
#include <semaphore.h>
#else
#include <posix/semaphore.h>
#endif

#include "gpio-gb.h"

LOG_MODULE_REGISTER(greybus_gpio, LOG_LEVEL_INF);

#define GB_GPIO_VERSION_MAJOR 0
#define GB_GPIO_VERSION_MINOR 1

static uint8_t gb_gpio_protocol_version(struct gb_operation *operation)
{
	struct gb_gpio_proto_version_response *response;

	response = gb_operation_alloc_response(operation, sizeof(*response));
	if (!response)
		return GB_OP_NO_MEMORY;

	response->major = GB_GPIO_VERSION_MAJOR;
	response->minor = GB_GPIO_VERSION_MINOR;
	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_line_count(struct gb_operation *operation)
{
	struct gb_gpio_line_count_response *response;
	struct device *dev;
	const struct gpio_driver_config *cfg;
	uint8_t count;

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	response = gb_operation_alloc_response(operation, sizeof(*response));
	if (!response)
		return GB_OP_NO_MEMORY;

	count = popcount(cfg->port_pin_mask);
	if (!count)
		return GB_OP_UNKNOWN_ERROR;

	response->count = count - 1;

	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_activate(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_activate_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	/* No "activation" in Zephyr. Maybe power mgmt in the future */

	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_deactivate(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_activate_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	/* No "deactivation" in Zephyr. Maybe power mgmt in the future */

	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_get_direction(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_get_direction_response *response;
	struct gb_gpio_get_direction_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	response = gb_operation_alloc_response(operation, sizeof(*response));
	if (!response)
		return GB_OP_NO_MEMORY;

	bool dir = gpio_pin_get_direction(dev, request->which);
	/* In Greybus 0 := output, 1 := input. Zephyr is the opposite */
	response->direction = !dir;
	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_direction_in(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_direction_in_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	return gb_errno_to_op_result(gpio_pin_configure(dev, (gpio_pin_t)request->which, GPIO_INPUT));
}

static uint8_t gb_gpio_direction_out(struct gb_operation *operation)
{
	int ret;
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_direction_out_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	ret = gpio_pin_configure(dev, request->which, GPIO_OUTPUT);
	if (ret != 0) {
		return gb_errno_to_op_result(-ret);
	}

	ret = gpio_pin_set(dev, request->which, request->value);
	if (ret != 0) {
		return gb_errno_to_op_result(-ret);
	}

	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_get_value(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_get_value_response *response;
	struct gb_gpio_get_value_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	response = gb_operation_alloc_response(operation, sizeof(*response));
	if (!response)
		return GB_OP_NO_MEMORY;

	response->value = gpio_pin_get(dev, (gpio_pin_t)request->which);
	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_set_value(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_set_value_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	return gb_errno_to_op_result(gpio_pin_set(dev, request->which, request->value));
}

static uint8_t gb_gpio_set_debounce(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_set_debounce_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	if (sys_le16_to_cpu(request->usec) > 0) {
		return gb_errno_to_op_result(gpio_pin_configure(dev, (gpio_pin_t)request->which, GPIO_INT_DEBOUNCE));
	}

	return GB_OP_SUCCESS;
}

static uint8_t gb_gpio_irq_mask(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_irq_mask_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	return gb_errno_to_op_result(gpio_pin_interrupt_configure(dev, request->which, GPIO_INT_DISABLE));
}

static uint8_t gb_gpio_irq_unmask(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_irq_unmask_request *request =
		gb_operation_get_request_payload(operation);

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	return gb_errno_to_op_result(gpio_pin_interrupt_configure(dev, request->which, GPIO_INT_ENABLE | GPIO_INT_EDGE_RISING));
}

int gb_gpio_irq_event(int irq, void *context, void *priv)
{
	struct gb_gpio_irq_event_request *request;
	struct gb_operation *operation;
	struct device *dev = (struct device *)context;
	int cport = gb_device_to_cport(dev);

	if (cport < 0) {
		return -EINVAL;
	}

	operation = gb_operation_create(cport, GB_GPIO_TYPE_IRQ_EVENT,
									sizeof(*request));
	if (!operation)
		return OK;

	request = gb_operation_get_request_payload(operation);
	request->which = irq;

	/* Host is responsible for unmasking. */
	gpio_pin_interrupt_configure(dev, request->which, GPIO_INT_MODE_DISABLED);

	/* Send unidirectional operation. */
	gb_operation_send_request_nowait(operation, NULL, false);

	gb_operation_destroy(operation);

	return OK;
}

static uint8_t gb_gpio_irq_type(struct gb_operation *operation)
{
	struct device *dev;
	const struct gpio_driver_config *cfg;
	struct gb_gpio_irq_type_request *request =
		gb_operation_get_request_payload(operation);
	enum gpio_int_mode mode;
	enum gpio_int_trig trigger;

	dev = gb_cport_to_device(operation->cport);
	if (dev == NULL) {
		return GB_OP_INVALID;
	}

	cfg = (const struct gpio_driver_config *)dev->config;
	__ASSERT_NO_MSG(cfg != NULL);

	if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return GB_OP_INVALID;
	}

	if (request->which >= popcount(cfg->port_pin_mask))
		return GB_OP_INVALID;

	switch(request->type) {
	case GB_GPIO_IRQ_TYPE_NONE:
		mode = GPIO_INT_MODE_DISABLED;
		trigger = 0;
		break;
	case GB_GPIO_IRQ_TYPE_EDGE_RISING:
		mode = GPIO_INT_MODE_EDGE;
		trigger = GPIO_INT_TRIG_HIGH;
		break;
	case GB_GPIO_IRQ_TYPE_EDGE_FALLING:
		mode = GPIO_INT_MODE_EDGE;
		trigger = GPIO_INT_TRIG_LOW;
		break;
	case GB_GPIO_IRQ_TYPE_EDGE_BOTH:
		mode = GPIO_INT_MODE_EDGE;
		trigger = GPIO_INT_TRIG_BOTH;
		break;
	case GB_GPIO_IRQ_TYPE_LEVEL_HIGH:
		mode = GPIO_INT_MODE_LEVEL;
		trigger = GPIO_INT_TRIG_HIGH;
		break;
	case GB_GPIO_IRQ_TYPE_LEVEL_LOW:
		mode = GPIO_INT_MODE_LEVEL;
		trigger = GPIO_INT_TRIG_HIGH;
		break;
	default:
		return GB_OP_INVALID;
	}

	return gb_errno_to_op_result(gpio_pin_interrupt_configure(dev, request->which, mode | trigger));
}

static struct gb_operation_handler gb_gpio_handlers[] = {
	GB_HANDLER(GB_GPIO_TYPE_PROTOCOL_VERSION, gb_gpio_protocol_version),
	GB_HANDLER(GB_GPIO_TYPE_LINE_COUNT, gb_gpio_line_count),
	GB_HANDLER(GB_GPIO_TYPE_ACTIVATE, gb_gpio_activate),
	GB_HANDLER(GB_GPIO_TYPE_DEACTIVATE, gb_gpio_deactivate),
	GB_HANDLER(GB_GPIO_TYPE_GET_DIRECTION, gb_gpio_get_direction),
	GB_HANDLER(GB_GPIO_TYPE_DIRECTION_IN, gb_gpio_direction_in),
	GB_HANDLER(GB_GPIO_TYPE_DIRECTION_OUT, gb_gpio_direction_out),
	GB_HANDLER(GB_GPIO_TYPE_GET_VALUE, gb_gpio_get_value),
	GB_HANDLER(GB_GPIO_TYPE_SET_VALUE, gb_gpio_set_value),
	GB_HANDLER(GB_GPIO_TYPE_SET_DEBOUNCE, gb_gpio_set_debounce),
	GB_HANDLER(GB_GPIO_TYPE_IRQ_TYPE, gb_gpio_irq_type),
	GB_HANDLER(GB_GPIO_TYPE_IRQ_MASK, gb_gpio_irq_mask),
	GB_HANDLER(GB_GPIO_TYPE_IRQ_UNMASK, gb_gpio_irq_unmask),
};

struct gb_driver gpio_driver = {
	.op_handlers = (struct gb_operation_handler*) gb_gpio_handlers,
	.op_handlers_count = ARRAY_SIZE(gb_gpio_handlers),
};

void gb_gpio_register(int cport, int bundle)
{
	gb_register_driver(cport, bundle, &gpio_driver);
}
