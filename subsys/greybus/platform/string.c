#include <stdint.h>
#include <zephyr.h>

#define DT_DRV_COMPAT zephyr_greybus_string
#include <device.h>

#define LOG_LEVEL CONFIG_GB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_string);

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

extern int gb_service_defer_init(const struct device *, int (*init)(const struct device *));
static int defer_greybus_string_init(const struct device *dev) {
	return gb_service_defer_init(dev, &greybus_string_init);
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
        DEVICE_INIT(greybus_string_##_num, "GBSTRING_" #_num,			\
        		defer_greybus_string_init, NULL,               	\
                            &greybus_string_config_##_num, POST_KERNEL,	\
                            CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_STRING);
