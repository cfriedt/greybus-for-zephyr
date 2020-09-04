/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TEST_SPI_H__
#define __TEST_SPI_H__

#include <devicetree.h>

#if DT_NODE_HAS_STATUS(DT_ALIAS(spi_0), okay)
#define SPI_DEV_NAME	DT_LABEL(DT_ALIAS(spi_0))
#elif DT_NODE_HAS_STATUS(DT_ALIAS(spi_1), okay)
#define SPI_DEV_NAME	DT_LABEL(DT_ALIAS(spi_1))
#elif DT_NODE_HAS_STATUS(DT_ALIAS(spi_2), okay)
#define SPI_DEV_NAME	DT_LABEL(DT_ALIAS(spi_2))
#else
#error "Please set the correct SPI device"
#endif

#endif /* __TEST_SPI_H__ */
