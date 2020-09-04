/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TESTS_SUBSYS_TEST_GREYBUS_GPIO_H_
#define TESTS_SUBSYS_TEST_GREYBUS_GPIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <ztest.h>

#if DT_NODE_HAS_STATUS(DT_INST(0, test_greybus_gpio), okay)
/* Execution of the test requires hardware configuration described in
 * devicetree.  See the test,gpio_basic_api binding local to this test
 * for details.
 *
 * If this is not present devices that have gpio-0, gpio-1, or gpio-2
 * aliases are supported for build-only tests.
 */
#define GPIO_DEV_NAME DT_GPIO_LABEL(DT_INST(0, test_greybus_gpio), out_gpios)
#define GPIO_PIN_OUT DT_GPIO_PIN(DT_INST(0, test_greybus_gpio), out_gpios)
#define GPIO_PIN_IN DT_GPIO_PIN(DT_INST(0, test_greybus_gpio), in_gpios)

#elif DT_NODE_HAS_STATUS(DT_ALIAS(gpio_0), okay)
#define GPIO_DEV_NAME DT_LABEL(DT_ALIAS(gpio_0))
#elif DT_NODE_HAS_STATUS(DT_ALIAS(gpio_1), okay)
#define GPIO_DEV_NAME DT_LABEL(DT_ALIAS(gpio_1))
#elif DT_NODE_HAS_STATUS(DT_ALIAS(gpio_3), okay)
#define GPIO_DEV_NAME DT_LABEL(DT_ALIAS(gpio_3))
#else
#error Unsupported board
#endif

#ifndef GPIO_PIN_OUT
/* For build-only testing use fixed pins. */
#define GPIO_PIN_OUT 2
#define GPIO_PIN_IN 3
#endif

#ifdef __cplusplus
}
#endif

#endif /* TESTS_SUBSYS_TEST_GREYBUS_GPIO_H_ */
