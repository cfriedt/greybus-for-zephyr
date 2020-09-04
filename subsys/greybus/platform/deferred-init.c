#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * FIXME: currently, Zephyr does not guarantee that parent DeviceTree nodes are
 * initialized before child DeviceTree nodes. This has the unfortunate side
 * effect of pushing-off device initialization to the application.
 * It's possible that Zephyr may initialize things in the correct order if
 * phandles are used instead, but that isn't clear at this point.
 *
 * E: greybus bundle: failed to get driver_api for device 'GREYBUS_0'
 * E: greybus bundle: failed to get driver_api for device 'GREYBUS_0'
 * I: probed greybus: 0 major: 0 minor: 1
 * I: probed cport 0: class: 0 protocol: 0
 * I: probed cport 0: class: 10 protocol: 2
 * E: failed to get driver_api for device 'GREYBUS_0'
 * E: greybus string: driver_api was NULL
 * E: greybus string: driver_api was NULL
 * E: Cannot create zeth (-1)
 * *** Booting Zephyr OS build zephyr-v2.3.0-733-g5217cd775382  ***
 * I: Initializing network
 * I: IPv4 address: 192.0.2.1
 * I: test_main(): ahoy!
 * I: IPv6 address: fe80::5eff:fe00:533b
 * I: IPv6 address: fe80::5eff:fe00:533b
 * ^C
 *
 * pabigot is working on a PR to address this, which reorders custom
 * elf-sections at link-time in order to create the proper order.
 *
 * See https://github.com/zephyrproject-rtos/zephyr/pull/26616 for details.
 */

struct definit {
	struct device *dev;
	int (*init)(struct device *);
};
static size_t definits_size;
static struct definit *definits;

int gb_service_defer_init(struct device *dev, int (*init)(struct device *)) {
	size_t n = definits_size;
	struct definit *tmp;

	tmp = realloc(definits, (n + 1) * sizeof(*tmp));
	if (NULL == tmp) {
		return -ENOMEM;
	}

	definits = tmp;
	definits[n].dev = dev;
	definits[n].init = init;

	definits_size++;

	return 0;
}

int gb_service_deferred_init(void) {
	int r = 0;
	int rr = 0;
	struct device *dev;
	int (*init)(struct device *);

	for(size_t i = 0; i < definits_size; ++i) {

		dev = definits[i].dev;
		init = definits[i].init;

		r = init(dev);
		if (r < 0) {
			if (0 == rr) {
				rr = r;
			}
		}
	}

	if (NULL != definits) {
		free(definits);
		definits = NULL;
		definits_size = 0;
	}

	return rr;
}

