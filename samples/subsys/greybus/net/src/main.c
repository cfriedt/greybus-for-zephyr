/*
 * Copyright (c) 2109 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr.h>

struct device;
extern int greybus_service_init(struct device *bus);

void main(void)
{
        int r = greybus_service_init(NULL);
        if (r < 0) {
                printf("gb_service_deferred_init() failed: %d\n", r);
        }
}
