/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <zephyr.h>

#define DT_DRV_COMPAT zephyr_greybus_interface
#include <device.h>
#include <devicetree.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_interface, CONFIG_GREYBUS_LOG_LEVEL);

struct greybus_interface_config {
	const uint8_t num;
	const uint16_t vendor_string_id;
	const uint16_t product_string_id;
	const char *const bus_name;
};

static int greybus_interface_init(const struct device *dev) {

	const struct greybus_interface_config *config =
		(struct greybus_interface_config *)dev->config;
	const struct device *bus;

	bus = device_get_binding(config->bus_name);
	if (NULL == bus) {
		LOG_ERR("failed to get binding for device '%s'", config->bus_name);
		return -ENODEV;
	}

	LOG_DBG("probed greybus interface %u", config->num);

    return 0;
}

extern int gb_service_defer_init(const struct device *, int (*init)(const struct device *));
static int defer_greybus_interface_init(const struct device *dev) {
	return gb_service_defer_init(dev, &greybus_interface_init);
}


#define DEFINE_GREYBUS_INTERFACE(_num)						\
															\
        static const struct greybus_interface_config		\
			greybus_interface_config_##_num = {				\
			.num = (uint8_t) _num,							\
			.vendor_string_id =								\
				DT_PROP(DT_PHANDLE(DT_DRV_INST(_num), 		\
					vendor_string_id), id),					\
			.product_string_id =							\
				DT_PROP(DT_PHANDLE(DT_DRV_INST(_num), 		\
					product_string_id), id),				\
			.bus_name = 									\
				DT_LABEL(DT_PARENT(DT_DRV_INST(_num))),		\
        };													\
        													\
        DEVICE_INIT(greybus_interface_##_num,				\
			"GBINTERFACE_" #_num,							\
			defer_greybus_interface_init,					\
			NULL,											\
			&greybus_interface_config_##_num, POST_KERNEL,	\
			CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_INTERFACE);
