/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <zephyr.h>

#define DT_DRV_COMPAT zephyr_greybus_string
#include <device.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_string, CONFIG_GREYBUS_LOG_LEVEL);

struct greybus_string_config {
    const uint16_t id;
    const char *const string_;
    const char *const bus_name;
};

static int greybus_string_init(const struct device *dev) {
	const struct greybus_string_config *config =
			(const struct greybus_string_config *)dev->config;
	const struct device *bus;

	bus = device_get_binding(config->bus_name);
	if (NULL == bus) {
		LOG_ERR("greybus string: device_get_binding() failed for '%s'", config->bus_name);
		return -EAGAIN;
	}

	LOG_DBG("probed greybus string %u: %s", config->id, config->string_);

    return 0;
}

#define DEFINE_GREYBUS_STRING(_num)                                     \
																		\
        static const struct greybus_string_config 						\
			greybus_string_config_##_num = {      						\
				.id = (uint16_t)DT_INST_PROP(_num, id),					\
				.string_ = DT_INST_PROP(_num, greybus_string),			\
				.bus_name = 											\
					DT_LABEL(DT_PARENT(DT_DRV_INST(_num))),				\
        };                                                              \
                                                                        \
		DEVICE_DT_INST_DEFINE(_num,										\
        					greybus_string_init, NULL, NULL,		\
                            &greybus_string_config_##_num, POST_KERNEL,	\
                            CONFIG_GREYBUS_STRING_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_STRING);
