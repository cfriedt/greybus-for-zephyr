/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TESTS_SUBSYS_TEST_GREYBUS_I2C_H_
#define TESTS_SUBSYS_TEST_GREYBUS_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <drivers/i2c.h>
#include <sys/util.h>
#include <ztest.h>

#if DT_NODE_HAS_STATUS(DT_INST(0, test_greybus_i2c), okay)
/* Execution of the test requires hardware configuration described in
 * devicetree.  See the test,i2c_basic_api binding local to this test
 * for details.
 *
 * If this is not present devices that have i2c-0, i2c-1, or i2c-2
 * aliases are supported for build-only tests.
 */

#ifndef DT_I2C_LABEL_BY_IDX
#define DT_I2C_LABEL_BY_IDX(node_id, i2c_pha, idx) \
		DT_PROP_BY_PHANDLE_IDX(node_id, i2c_pha, idx, label)
#endif

#ifndef DT_I2C_LABEL
#define DT_I2C_LABEL(node_id, i2c_pha) \
		DT_I2C_LABEL_BY_IDX(node_id, i2c_pha, 0)
#endif

#define I2C_DEV_NAME DT_I2C_LABEL(DT_INST(0, test_greybus_i2c), test_i2c)

#elif DT_NODE_HAS_STATUS(DT_ALIAS(i2c_0), okay)
#define I2C_DEV_NAME DT_LABEL(DT_ALIAS(i2c_0))
#elif DT_NODE_HAS_STATUS(DT_ALIAS(i2c_1), okay)
#define I2C_DEV_NAME DT_LABEL(DT_ALIAS(i2c_1))
#elif DT_NODE_HAS_STATUS(DT_ALIAS(i2c_3), okay)
#define I2C_DEV_NAME DT_LABEL(DT_ALIAS(i2c_3))
#else
#error Unsupported board
#endif

#ifdef __cplusplus
}
#endif

#endif /* TESTS_SUBSYS_TEST_GREYBUS_I2C_H_ */
