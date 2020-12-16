#include <stdint.h>
#include <zephyr.h>

#define DT_DRV_COMPAT zephyr_greybus_bundle
#include <device.h>
#include <devicetree.h>

#define LOG_LEVEL CONFIG_GB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_bundle);

struct greybus_bundle_config {
    const uint8_t id;
    const uint16_t class_;
    const char *const bus_name;
};

static int greybus_bundle_init(const struct device *dev) {

	const struct greybus_bundle_config *const config =
			(const struct greybus_bundle_config *)dev->config;

	const struct device *bus;

	bus = device_get_binding(config->bus_name);
	if (NULL == bus) {
		LOG_ERR("greybus bundle: failed to get binding for device '%s'", config->bus_name);
		return -ENODEV;
	}

	LOG_DBG("probed greybus bundle %u: class: %u", config->id, config->class_);

    return 0;
}

extern int gb_service_defer_init(const struct device *, int (*init)(const struct device *));
static int defer_greybus_bundle_init(const struct device *dev) {
	return gb_service_defer_init(dev, &greybus_bundle_init);
}

#define DEFINE_GREYBUS_BUNDLE(_num)                                     \
																		\
        static const struct greybus_bundle_config 						\
			greybus_bundle_config_##_num = {      						\
				.id = DT_INST_PROP(_num, id),							\
				.class_ = DT_INST_PROP(_num, bundle_class),				\
				.bus_name = 											\
					DT_LABEL(DT_PARENT(DT_DRV_INST(_num))),				\
        };                                                              \
                                                                        \
        DEVICE_DT_INST_DEFINE(_num,										\
                            defer_greybus_bundle_init,					\
							NULL, NULL,									\
                            &greybus_bundle_config_##_num,				\
							POST_KERNEL,								\
                            CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_BUNDLE);
