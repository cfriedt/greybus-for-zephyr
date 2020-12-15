/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr.h>
#include <sys/byteorder.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_manifest, CONFIG_GREYBUS_LOG_LEVEL);

#ifdef CONFIG_GREYBUS_MANIFEST_BUILTIN
static const unsigned char greybus_manifest_builtin[] = {
#include "greybus_mnfb.inc"
};
#else
#define greybus_manifest_builtin NULL
#endif /* CONFIG_GREYBUS_MANIFEST_BUILTIN */

#include "../greybus-manifest.h"

int manifest_get(uint8_t **mnfb, size_t *mnfb_size)
{
	int r = -ENOENT;

	if (IS_ENABLED(CONFIG_GREYBUS_MANIFEST_BUILTIN)) {
		*mnfb = greybus_manifest_builtin;
		*mnfb_size = sizeof(greybus_manifest_builtin);
		r = 0;
	}

	return r;
}
