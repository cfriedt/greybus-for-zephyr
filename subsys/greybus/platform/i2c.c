#include <drivers/i2c.h>
#include <dt-bindings/greybus/greybus.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <stdint.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#define DT_DRV_COMPAT zephyr_greybus_i2c_controller
#include <device.h>

#define LOG_LEVEL CONFIG_GB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_i2c_control);

#include "../i2c-gb.h"
#include "transport.h"

struct greybus_i2c_control_config {
    const uint8_t id;
    const uint8_t bundle;
    const char *const greybus_i2c_controller_name;
    const char *const bus_name;
};

struct greybus_i2c_control_data {
    struct device *greybus_i2c_controller;
};

static int greybus_i2c_control_init(struct device *dev) {

	struct greybus_i2c_control_data *drv_data =
        (struct greybus_i2c_control_data *)dev->data;
    struct greybus_i2c_control_config *config =
        (struct greybus_i2c_control_config *)dev->config;
    int r;
    struct device *bus;
    struct greybus_platform_api *api;

    drv_data->greybus_i2c_controller =
        device_get_binding(config->greybus_i2c_controller_name);
    if (NULL == drv_data->greybus_i2c_controller) {
		LOG_ERR("i2c control: failed to get binding for device '%s'",
			config->greybus_i2c_controller_name);
		return -ENODEV;
    }

    bus = device_get_binding(config->bus_name);
    if (NULL == bus) {
		LOG_ERR("i2c control: failed to get binding for device '%s'", config->bus_name);
    	return -ENODEV;
    }

    api = (struct greybus_platform_api *) bus->api;
    if (NULL == api) {
		LOG_ERR("i2c control: failed to get api for '%s'", config->bus_name);
    	return -EINVAL;
    }

    r = api->add_cport(bus, config->id, config->bundle, CPORT_PROTOCOL_I2C);
    if (r < 0) {
		LOG_ERR("i2c control: failed to get api for '%s'", config->bus_name);
		return r;
    }

    r = gb_add_cport_device_mapping(config->id, drv_data->greybus_i2c_controller);
    if (r < 0) {
		LOG_ERR("i2c control: failed to add mapping between %u and %s", config->id, dev->name);
		return r;
    }

    LOG_DBG("probed cport %u: bundle: %u protocol: %u", config->id,
		config->bundle, CPORT_PROTOCOL_I2C);

    return 0;
}

extern int gb_service_defer_init(struct device *, int (*init)(struct device *));
static int defer_greybus_i2c_control_init(struct device *dev) {
	return gb_service_defer_init(dev, &greybus_i2c_control_init);
}

#define DEFINE_GREYBUS_I2C_CONTROL(_num)										\
																				\
		BUILD_ASSERT(DT_PROP(DT_PARENT(DT_DRV_INST(_num)), bundle_class)		\
		== BUNDLE_CLASS_BRIDGED_PHY, "BUNDLE_CLASS_BRIDGED_PHY required"); 		\
																				\
		BUILD_ASSERT(DT_PROP(DT_DRV_INST(_num), cport_protocol) 				\
		== CPORT_PROTOCOL_I2C, "CPORT_PROTOCOL_I2C required"); 				\
																				\
		static struct greybus_i2c_control_config								\
			greybus_i2c_control_config_##_num = {								\
                .id = (uint8_t)DT_INST_PROP(_num, id), \
                .bundle = (uint8_t)DT_PROP(DT_PARENT(DT_DRV_INST(_num)), id), \
				.greybus_i2c_controller_name = 								\
                    DT_LABEL(DT_PHANDLE(DT_DRV_INST(_num), 						\
                    		greybus_i2c_controller)), 							\
				.bus_name = 									\
					DT_LABEL(DT_PARENT(DT_PARENT(DT_DRV_INST(_num)))),		\
        };																		\
        																		\
        static struct greybus_i2c_control_data									\
			greybus_i2c_control_data_##_num;									\
        																		\
        DEVICE_INIT(i2c_i2c_control_##_num, "GBI2C_" #_num,					\
                            defer_greybus_i2c_control_init,					\
							&greybus_i2c_control_data_##_num,					\
                            &greybus_i2c_control_config_##_num, POST_KERNEL,	\
                            CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_I2C_CONTROL);
