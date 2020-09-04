/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

extern void test_greybus_spi_protocol_version(void);
extern void test_greybus_spi_cport_shutdown(void);
extern void test_greybus_spi_master_config(void);
extern void test_greybus_spi_device_config(void);
extern void test_greybus_spi_transfer(void);

static void board_setup(void)
{
	if (IS_ENABLED(CONFIG_SPI_SIM)) {
		extern void spi_sim_setup(void);
		spi_sim_setup();
	}
	if (IS_ENABLED(CONFIG_GPIO_SIM)) {
		extern void gpio_sim_setup(void);
		gpio_sim_setup();
	}
}

extern void test_greybus_setup(void);
extern void test_greybus_teardown(void);

void test_main(void) {

	board_setup();
	test_greybus_setup();
    ztest_test_suite(greybus_spi,
        ztest_unit_test(test_greybus_spi_protocol_version),
        ztest_unit_test(test_greybus_spi_cport_shutdown),
        ztest_unit_test(test_greybus_spi_master_config),
        ztest_unit_test(test_greybus_spi_device_config),
        ztest_unit_test(test_greybus_spi_transfer)
        );
    ztest_run_test_suite(greybus_spi);
    test_greybus_teardown();
}
