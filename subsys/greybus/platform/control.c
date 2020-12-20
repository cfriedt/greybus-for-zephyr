#include <stdint.h>
#include <zephyr.h>
#include <dt-bindings/greybus/greybus.h>

#define DT_DRV_COMPAT zephyr_greybus_control
#include <device.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_control, CONFIG_GREYBUS_LOG_LEVEL);

struct greybus_control_config {
    const uint8_t id;
    const uint8_t bundle;
    const char *bus_name;
};

static int greybus_control_init(const struct device *dev) {

    struct greybus_control_config *config =
        (struct greybus_control_config *)dev->config;
    const struct device *bus;

    bus = device_get_binding(config->bus_name);
    if (NULL == bus) {
	LOG_ERR("control: failed to get binding for device '%s'", config->bus_name);
    	return -ENODEV;
    }

    LOG_DBG("probed cport %u: bundle: %u protocol: %u", config->id,
		config->bundle, CPORT_PROTOCOL_CONTROL);

    return 0;
}

#define DEFINE_GREYBUS_CONTROL(_num)										\
																			\
		BUILD_ASSERT(DT_PROP(DT_PARENT(DT_DRV_INST(_num)), bundle_class)	\
			== BUNDLE_CLASS_CONTROL, "BUNDLE_CLASS_CONTROL required"); 		\
																			\
		BUILD_ASSERT(DT_PROP(DT_DRV_INST(_num), cport_protocol) 			\
			== CPORT_PROTOCOL_CONTROL, "CPORT_PROTOCOL_CONTROL required"); 	\
																			\
		static const struct greybus_control_config							\
			greybus_control_config_##_num = {								\
                .id = (uint8_t) _num,										\
                .bundle = (uint8_t)DT_PROP(DT_PARENT(DT_DRV_INST(_num)), id), \
				.bus_name = 												\
					DT_LABEL(DT_PARENT(DT_PARENT(DT_DRV_INST(_num)))),		\
        };																	\
        																	\
		DEVICE_DT_INST_DEFINE(_num,											\
                            greybus_control_init,						\
							NULL, NULL,										\
                            &greybus_control_config_##_num, POST_KERNEL,	\
                            CONFIG_GREYBUS_CPORT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS_CONTROL);
