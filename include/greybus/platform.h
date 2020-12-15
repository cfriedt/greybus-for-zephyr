/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_INCLUDE_GREYBUS_PLATFORM_H_
#define ZEPHYR_INCLUDE_GREYBUS_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

struct device;

/**
 * @brief Map a Greybus @p cport to / from a Zephyr @ref device
 *
 * This function creates a bidirectional mapping between @p cport
 * and @p dev. And is intended to be used by Zephyr's platform-
 * independent device backend.
 *
 * @param cport the cport to be used in the mapping
 * @param dev   the device to be used in the mapping
 * @return 0 on success
 * @return -EINVAL if an argument is invalid
 * @return -ENOMEM if memory could not be allocated
 * @return -EALREADY if at least one of @p cport or @dev has already been mapped
 */
int gb_add_cport_device_mapping(unsigned int cport, const struct device *dev);

/**
 * @brief Query the Greybus cport / device mapping for @p dev
 *
 * This function retrieves the cport associated with a particular
 * Zephyr @ref device.
 *
 * @param dev a pointer to the Zephyr @ref device to use for the query
 * @return the associated cport on success (which is >= 0)
 * @return -ENOENT on failure
 */
int gb_device_to_cport(const struct device *dev);

/**
 * @brief Query the Greybus cport / device mapping for @p cport
 *
 * This function retrieves the @ref device associated with a
 * particular Greybus @p cport.
 *
 * @param cport the Greybus cport use for the query
 * @return a pointer to the associated Zephyr @ref device on success
 * @return NULL on failure
 */
const struct device *gb_cport_to_device(unsigned int cport);

struct gb_spi_master_config_response;
struct gb_spi_device_config_response;
struct spi_cs_control;
struct gb_platform_spi_api {
	int (*controller_config_response)(const struct device *dev, struct gb_spi_master_config_response *rsp);
	int (*num_peripherals)(const struct device *dev);
	int (*peripheral_config_response)(const struct device *dev, uint8_t chip_select, struct gb_spi_device_config_response *rsp);
	int (*get_cs_control)(const struct device *dev, uint8_t chip_select, struct spi_cs_control *ctrl);
};

/*
 * Get the greybus spi device associated with a physical spi device
 *
 * usage:
 *
 * const struct device *gb_spidev = gb_spidev_from_zephyr_spidev(device_get_binding("SPI_0"));
 * const struct gb_platform_spi_api *api = (struct gb_platform_spi_api *)gb_spidev->driver_api;
 */
const struct device *gb_spidev_from_zephyr_spidev(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_GREYBUS_PLATFORM_H_ */
