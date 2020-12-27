/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <device.h>
#include <init.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <greybus-utils/manifest.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_service, CONFIG_GREYBUS_LOG_LEVEL);

#include "transport.h"
#include "manifest.h"
#include "certificate.h"

/* Currently only one greybus instance is supported */
#define GREYBUS_BUS_NAME "GREYBUS_0"

static struct gb_transport_backend *xport;
static size_t num_cports;

unsigned int unipro_cport_count(void)
{
    return num_cports;
}

const struct gb_transport_backend *
gb_transport_get_backend(void)
{
	return xport;
}

static int greybus_service_init(const struct device *bus)
{
	int r;
	uint8_t *mnfb;
	size_t mnfb_size;
	unsigned int *cports = NULL;

	if (xport != NULL) {
		LOG_ERR("service already initialized");
		return -EALREADY;
	}

	r = greybus_tls_init();
	if (r < 0) {
		LOG_ERR("gb_tls_init() failed: %d", r);
		goto out;
	}

	LOG_DBG("Greybus initializing..");

	bus = device_get_binding(GREYBUS_BUS_NAME);
	if (NULL == bus) {
		r = -ENODEV;
		LOG_ERR("failed to get " GREYBUS_BUS_NAME " device");
		r = -ENODEV;
		goto out;
	}

	r = manifest_get(&mnfb, &mnfb_size);
	if (r < 0) {
		LOG_ERR("failed to get mnfb");
		goto out;
	}

	r = manifest_parse(mnfb, mnfb_size);
	if (r != true) {
		LOG_ERR("failed to parse mnfb");
		r = -EINVAL;
		goto out;
	}

	extern size_t manifest_get_num_cports(void);
	num_cports = manifest_get_num_cports();
    if (num_cports == 0) {
		LOG_ERR("no cports are defined");
        r = -EINVAL;
		goto out;
    }

    xport = gb_transport_backend_init(num_cports);
    if (xport == NULL) {
        LOG_ERR("failed to get transport");
        r = -EIO;
        goto out;
    }

    set_manifest_blob(mnfb);

    r = gb_init((struct gb_transport_backend *) xport);
    if (r < 0) {
        LOG_ERR("gb_init() failed: %d", r);
        goto clear_mnfb;
    }

    enable_cports();

    LOG_INF("Greybus is active");

    r = 0;
    goto out;

clear_mnfb:
    set_manifest_blob(NULL);

out:
    if (cports != NULL) {
        free(cports);
    }

    return r;
}

SYS_INIT(greybus_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
