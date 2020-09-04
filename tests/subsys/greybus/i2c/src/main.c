/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

extern void test_greybus_i2c_protocol_version(void);
extern void test_greybus_i2c_cport_shutdown(void);
extern void test_greybus_i2c_functionality(void);
extern void test_greybus_i2c_transfer(void);

static void board_setup(void)
{
	if (IS_ENABLED(CONFIG_I2C_SIM)) {
		extern void i2c_sim_setup(void);
		i2c_sim_setup();
	}
}

extern void test_greybus_setup(void);
extern void test_greybus_teardown(void);

void test_main(void) {

	board_setup();
	test_greybus_setup();
    ztest_test_suite(greybus_i2c,
        ztest_unit_test(test_greybus_i2c_protocol_version),
        ztest_unit_test(test_greybus_i2c_cport_shutdown),
        ztest_unit_test(test_greybus_i2c_functionality),
        ztest_unit_test(test_greybus_i2c_transfer)
        );
    ztest_run_test_suite(greybus_i2c);
    test_greybus_teardown();
}
