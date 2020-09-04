#include <stdint.h>
#include <zephyr.h>
#include <greybus/manifecto/manifest.h>

#define DT_DRV_COMPAT zephyr_greybus
#include <device.h>

#define LOG_LEVEL CONFIG_GB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_platform_bus);

#include <greybus/platform.h>

struct greybus_config {
    const uint8_t id;
    const uint8_t version_major;
    const uint8_t version_minor;
};

struct greybus_data {
	manifest_t manifest;
};

static int greybus_init(struct device *bus) {

	const struct greybus_config *const config =
			(const struct greybus_config *)bus->config;
	struct greybus_data *const data =
			(struct greybus_data *)bus->data;
	int r;

	data->manifest = manifest_new();
	if (NULL == data->manifest) {
		LOG_ERR("manifest_new() failed");
		return -ENOMEM;
	}

	r = manifest_add_header(data->manifest, config->version_major, config->version_minor);
	if (r < 0) {
		LOG_ERR("manifest_add_header() failed: %d", r);
		return r;
	}

	LOG_DBG("probed greybus: %u major: %u minor: %u",
		config->id, config->version_major, config->version_minor);

    return 0;
}

static int greybus_add_interface(struct device *bus, uint16_t vendor_string_id,
		uint16_t product_string_id) {

	struct greybus_data *const data =
			(struct greybus_data *)bus->data;

	return manifest_add_interface_desc(data->manifest, vendor_string_id, product_string_id);
}

static int greybus_add_string(struct device *bus, uint8_t id, const char *string_) {

	struct greybus_data *const data =
			(struct greybus_data *)bus->data;

	return manifest_add_string_desc(data->manifest, id, string_);
}

static int greybus_add_bundle(struct device *bus, uint8_t id, BundleClass class_) {

	struct greybus_data *const data =
			(struct greybus_data *)bus->data;

	return manifest_add_bundle_desc(data->manifest, id, class_);
}

static int greybus_add_cport(struct device *bus, uint8_t id, BundleClass class_, CPortProtocol protocol) {

	struct greybus_data *const data =
			(struct greybus_data *)bus->data;

	return manifest_add_cport_desc(data->manifest, id, class_, protocol);
}

static int greybus_gen_mnfb(struct device *bus, uint8_t **mnfb, size_t *mnfb_size) {

	int r;
	struct greybus_data *const data =
			(struct greybus_data *)bus->data;

	r = manifest_mnfb_gen(data->manifest);
	if (r < 0) {
		return r;
	}

	return manifest_mnfb_give(data->manifest, mnfb, mnfb_size);
}

static int greybus_get_cports(struct device *bus, unsigned int **cports, size_t *num_cports) {
	struct greybus_data *const data =
		(struct greybus_data *)bus->data;

	return manifest_get_cports(data->manifest, cports, num_cports);
}

static void greybus_fini(struct device *bus) {

	struct greybus_data *const data =
			(struct greybus_data *)bus->data;

	manifest_fini(data->manifest);
}

static const struct greybus_platform_api platform_api = {
	.add_interface = greybus_add_interface,
	.add_string = greybus_add_string,
	.add_bundle = greybus_add_bundle,
	.add_cport = greybus_add_cport,
	.get_cports = greybus_get_cports,
	.gen_mnfb = greybus_gen_mnfb,
	.fini = greybus_fini,
};

#define DEFINE_GREYBUS(_num)                            \
														\
        static const struct greybus_config 				\
			greybus_config_##_num = {      				\
				.id = _num,                             \
				.version_major = 						\
					DT_INST_PROP(_num, version_major),	\
				.version_minor = 						\
					DT_INST_PROP(_num, version_minor),	\
        };												\
        												\
		static struct greybus_data greybus_data_##_num;	\
														\
        DEVICE_AND_API_INIT(greybus_##_num,				\
			"GREYBUS_" #_num,							\
			greybus_init, &greybus_data_##_num,			\
			&greybus_config_##_num, POST_KERNEL,		\
			CONFIG_KERNEL_INIT_PRIORITY_DEVICE,			\
			&platform_api);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS);
