#include <stdint.h>
#include <zephyr.h>
#include <greybus/manifecto/manifest.h>

#define DT_DRV_COMPAT zephyr_greybus_bundle
#include <device.h>
#include <devicetree.h>

#define LOG_LEVEL CONFIG_GB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_bundle);

#include <greybus/platform.h>

struct greybus_bundle_config {
    const uint8_t id;
    const BundleClass class_;
    const char *const bus_name;
};

static int greybus_bundle_init(struct device *dev) {

	const struct greybus_bundle_config *const config =
			(const struct greybus_bundle_config *)dev->config;

	int r;
	struct greybus_platform_api *api;
	struct device *bus;

	bus = device_get_binding(config->bus_name);
	if (NULL == bus) {
		LOG_ERR("greybus bundle: failed to get binding for device '%s'", config->bus_name);
		return -ENODEV;
	}

	api = (struct greybus_platform_api *)bus->api;
	if (NULL == api) {
		LOG_ERR("greybus bundle: failed to get api for device '%s'", config->bus_name);
		return -EINVAL;
	}

	r = api->add_bundle(bus, config->id, config->class_);
	if (r < 0) {
		LOG_ERR("greybus bundle: add_bundle() failed: %d", r);
		return r;
	}

	LOG_DBG("probed greybus bundle %u: class: %u", config->id, config->class_);

    return 0;
}

extern int gb_service_defer_init(struct device *, int (*init)(struct device *));
static int defer_greybus_bundle_init(struct device *dev) {
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
        DEVICE_INIT(greybus_bundle_##_num, "GBBUNDLE_" #_num,			\
                            defer_greybus_bundle_init,						\
							NULL,										\
                            &greybus_bundle_config_##_num,				\
							POST_KERNEL,								\
                            CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_BUNDLE);
