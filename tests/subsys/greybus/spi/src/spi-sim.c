/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_SPI_SIM

#include <drivers/spi.h>
#include <drivers/gpio.h>
#include <drivers/spi/spi_sim.h>
#include <drivers/eeprom.h>
#include <stdio.h>
#include <sys/util.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_spi_sim, CONFIG_SPI_LOG_LEVEL);

#ifndef SPI_DEV_NAME
#define SPI_DEV_NAME "SPI_0"
#endif

/* AT25 instruction set */
typedef enum {
	AT25_WRSR = 1, /* Write STATUS register        */
	AT25_WRITE = 2, /* Write data to memory array   */
	AT25_READ = 3, /* Read data from memory array  */
	AT25_WRDI = 4, /* Reset the write enable latch */
	AT25_RDSR = 5, /* Read STATUS register         */
	AT25_WREN = 6, /* Set the write enable latch   */
	AT25_EONE = 0x52, /* Erase One Sector in Memory Array */
	AT25_EALL = 0x62, /* Erase All Sectors in Memory Array */
	AT25_RDR = 0x15, /* Read Manufacturer and Product ID */
} at25_op_t;

/* AT25 status register bits */
typedef enum {
	AT25_STATUS_WIP = BIT(0), /* Write-In-Process   (RO) */
	AT25_STATUS_WEL = BIT(1), /* Write Enable Latch (RO) */
	AT25_STATUS_BP0 = BIT(2), /* Block Protection 0 (RW) */
	AT25_STATUS_BP1 = BIT(3), /* Block Protection 1 (RW) */
} at25_status_t;

static int spi_sim_callback(struct device *dev, const struct spi_config *config,
			    const struct spi_buf_set *tx_bufs,
			    const struct spi_buf_set *rx_bufs)
{
	static uint8_t status;
	static uint8_t data[256];
	static bool write;
	static uint32_t addr;
	const struct spi_buf *tx = &tx_bufs->buffers[0];
	const struct spi_buf *rx;
	uint8_t *x;
	size_t len;

	/* if there is a chip-select, then use it! */
	if (config->cs != NULL) {
		k_usleep(config->cs->delay);
		if ((config->operation & SPI_CS_ACTIVE_HIGH) != 0) {
			gpio_pin_set(config->cs->gpio_dev, config->cs->gpio_pin, 1);
		} else {
			gpio_pin_set(config->cs->gpio_dev, config->cs->gpio_pin, 0);
		}
	}

	at25_op_t op = ((uint8_t *)tx->buf)[0] & (~0x08);

	switch(op) {
	case AT25_WRSR:
		break;
	case AT25_WRITE:

		write = true;

		__ASSERT_NO_MSG(tx->len >= 4);

		addr = 0;
		/* A[23..17] are don't care bits for the 512k version */
		/* addr |= (uint32_t)(((uint8_t *)tx->buf)[0]) << 16; */
		/* A[16] should be a zero bit for the 512k version */
		addr |= ((uint32_t)(((uint8_t *)tx->buf)[1]) << 8) & 7;
		addr |= (uint32_t)(((uint8_t *)tx->buf)[2]) << 0;

		if (tx->len == 4) {
			__ASSERT_NO_MSG(tx_bufs->count > 1);
			tx = &tx_bufs->buffers[1];
			x = tx->buf;
			len = tx->len;
		} else {
			x = (uint8_t *)tx->buf + 4;
			len = tx->len - 4;
		}

		memcpy(&data[addr], x, len);

		write = false;

		break;

	case AT25_READ:

		write = false;

		addr = 0;
		/* A[23..17] are don't care bits for the 512k version */
		/* addr |= (uint32_t)(((uint8_t *)tx->buf)[0]) << 16; */
		/* A[16] should be a zero bit for the 512k version */
		addr |= ((uint32_t)(((uint8_t *)tx->buf)[1]) << 8) & 7;
		addr |= (uint32_t)(((uint8_t *)tx->buf)[2]) << 0;

		if (tx->len == 4) {
			__ASSERT_NO_MSG(rx_bufs->count > 1);
			rx = &rx_bufs->buffers[1];
			x = rx->buf;
			len = rx->len;
		} else {
			rx = &rx_bufs->buffers[0];
			x = (uint8_t *)rx->buf + 4;
			len = rx->len - 4;
		}

		memcpy(x, &data[addr], len);

		break;

	case AT25_WRDI:
		break;

	case AT25_RDSR:

		rx = &rx_bufs->buffers[0];
		memcpy(rx->buf, tx->buf, sizeof(rx->len));
		((uint8_t *)rx->buf)[1] = status;

		break;

	case AT25_WREN:
		status |= AT25_STATUS_WEL;
		break;
	case AT25_EONE:
		break;
	case AT25_EALL:
		break;
	case AT25_RDR:
		break;
	default:

		__ASSERT(1 == 0, "invalid opcode %u", op);

		break;
	}

	/* if there is a chip-select, then use it! */
	if (config->cs != NULL) {
		if ((config->operation & SPI_CS_ACTIVE_HIGH) != 0) {
			gpio_pin_set(config->cs->gpio_dev, config->cs->gpio_pin, 0);
		} else {
			gpio_pin_set(config->cs->gpio_dev, config->cs->gpio_pin, 1);
		}
		k_usleep(config->cs->delay);
	}

	return 0;
}

void spi_sim_setup(void)
{
	int r;
	struct device *dev;

	dev = device_get_binding(SPI_DEV_NAME);
	__ASSERT(dev != NULL, "failed to get binding for " SPI_DEV_NAME);

	r = spi_sim_callback_register(dev, 0, spi_sim_callback);
	__ASSERT(r == 0, "failed to register spi_sim callback: %d", r);
}

#endif /* CONFIG_SPI_SIM */
