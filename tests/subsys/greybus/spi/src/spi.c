/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <errno.h>
#include <greybus/greybus.h>
#include <string.h>
#include <sys/util.h>
#include <ztest.h>
#include <zephyr.h>

#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) ||                                \
	defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) ||                            \
	defined(CONFIG_BOARD_NRF52_BSIM)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/byteorder.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#else

#include <net/socket.h>
#include <posix/unistd.h>

#endif

#include "../../../../subsys/greybus/spi-gb.h"

#include "test-greybus-spi.h"

#define TIMEOUT_MS (-1)
#define PORT 4243

static struct device *spi_dev;

static int fd = -1;

static char *to_string(uint8_t *data, size_t len)
{
    static char buf[256];
    char *p = buf;

    memset(buf, '\0', sizeof(buf));
    for(size_t i = 0; i < len && p < buf + sizeof(buf) - 4; ++i) {
        sprintf(p, "%02x", data[i]);
        p += 2;
        if (i < len - 1) {
            *p++ = ',';
            *p++ = ' ';
        }
    }

    return buf;
}

void test_greybus_setup(void)
{
	struct sockaddr_in6 addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = in6addr_loopback,
		.sin6_port = htons(PORT),
	};
	socklen_t addrlen = sizeof(addr);
	int r;

	spi_dev = device_get_binding(SPI_DEV_NAME);
	zassert_not_equal(spi_dev, NULL,
			  "failed to get device binding for " SPI_DEV_NAME);

	r = socket(AF_INET6, SOCK_STREAM, 0);
	__ASSERT(r >= 0, "connect: %d", errno);
	fd = r;

	r = connect(fd, (const struct sockaddr *)&addr, addrlen);
	__ASSERT(r == 0, "connect: %d", errno);
}

void test_greybus_teardown(void)
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

static void tx_rx(const struct gb_operation_hdr *req,
		  struct gb_operation_hdr *rsp, size_t rsp_size)
{
	int r;
	int size;
	struct pollfd pollfd;

	size = sys_le16_to_cpu(req->size);
	r = send(fd, req, size, 0);
	zassert_not_equal(r, -1, "send: %s", errno);
	zassert_equal(r, size, "send: expected: %d actual: %d", size, r);

	pollfd.fd = fd;
	pollfd.events = POLLIN;

	r = poll(&pollfd, 1, TIMEOUT_MS);
	zassert_not_equal(r, -1, "poll: %s", errno);
	zassert_not_equal(r, 0, "timeout waiting for response");
	zassert_equal(r, 1, "invalid number of pollfds with data: %d", r);

	r = recv(fd, rsp, rsp_size, 0);
	zassert_not_equal(r, -1, "recv: %s", errno);
	if (r != rsp_size) {
		printk("rsp: [%s]", to_string((uint8_t *)rsp, r));
	}
	zassert_equal(rsp_size, r, "recv: expected: %u actual: %u",
		      (unsigned)rsp_size, r);

	zassert_equal(rsp->id, req->id,
			  "expected: 0x%04x actual: 0x%04x",
			  sys_le16_to_cpu(req->id),
			  sys_le16_to_cpu(rsp->id));
	zassert_equal(rsp->type, GB_TYPE_RESPONSE_FLAG | req->type,
			  "expected: %u actual: %u",
			  GB_TYPE_RESPONSE_FLAG | req->type, rsp->type);
}

void test_greybus_spi_protocol_version(void)
{
	const struct gb_operation_hdr req = {
		.size = sys_cpu_to_le16(sizeof(struct gb_operation_hdr)),
		.id = sys_cpu_to_le16(0xabcd),
		.type = GB_SPI_PROTOCOL_VERSION,
	};
	uint8_t rsp_[0 + sizeof(struct gb_operation_hdr) +
		     sizeof(struct gb_spi_proto_version_response)];
	const size_t rsp_size = sizeof(rsp_);
	struct gb_operation_hdr *const rsp =
		(struct gb_operation_hdr *)rsp_;
	struct gb_spi_proto_version_response *const pv_rsp =
		(struct gb_spi_proto_version_response
			 *)(rsp_ + sizeof(struct gb_operation_hdr));

	/* add some "noise" values that should be overwritten */
	pv_rsp->major = 0xf0;
	pv_rsp->minor = 0x0d;

	tx_rx(&req, rsp, rsp_size);

	zassert_equal(rsp->result, GB_OP_SUCCESS,
		      "expected: GB_OP_SUCCESS actual: %u",
		      rsp->result);

	/* GB_SPI_VERSION_MAJOR (0) is buried in subsys/greybus/spi.c */
	zassert_equal(pv_rsp->major, 0, "expected: %u actual: %u", 0, pv_rsp->major);

	/* GB_SPI_VERSION_MINOR (1) is buried in subsys/greybus/spi.c */
	zassert_equal(pv_rsp->minor, 1, "expected: %u actual: %u", 1, pv_rsp->minor);
}

void test_greybus_spi_cport_shutdown(void)
{
	/*
	 * looks like the original NuttX implementation was missing any kind
	 * of cport shutdown request
	 */
}

void test_greybus_spi_master_config(void)
{
	const struct gb_operation_hdr req = {
		.size = sys_cpu_to_le16(sizeof(struct gb_operation_hdr)),
		.id = sys_cpu_to_le16(0xabcd),
		.type = GB_SPI_TYPE_MASTER_CONFIG,
	};
	uint8_t rsp_[0 + sizeof(struct gb_operation_hdr) +
		     sizeof(struct gb_spi_master_config_response)];

	struct gb_operation_hdr *const rsp =
		(struct gb_operation_hdr *)rsp_;

	const size_t rsp_size = sizeof(rsp_);
	struct gb_spi_master_config_response *const master_rsp =
		(struct gb_spi_master_config_response
			 *)(rsp_ + sizeof(struct gb_operation_hdr));

	tx_rx(&req, rsp, rsp_size);

	zassert_equal(rsp->result, GB_OP_SUCCESS,
		      "expected: GB_OP_SUCCESS actual: %u",
		      rsp->result);

	/*
	 * Zephyr currently has no way to query these settings from the hardware
	 * but at least they are compile-time constant and can be extracted out
	 * of devicetree
	 */
	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, bpw_mask),
		sys_le32_to_cpu(master_rsp->bpw_mask),
		"bpw_mask: expected: %x actual: %x",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, bpw_mask),
		sys_le32_to_cpu(master_rsp->bpw_mask)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, min_speed_hz),
		sys_le32_to_cpu(master_rsp->min_speed_hz),
		"min_speed_hz: expected: %u actual: %u",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, min_speed_hz),
		sys_le32_to_cpu(master_rsp->min_speed_hz)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, max_speed_hz),
		sys_le32_to_cpu(master_rsp->max_speed_hz),
		"max_speed_hz: expected: %u actual: %u",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, max_speed_hz),
		sys_le32_to_cpu(master_rsp->max_speed_hz)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, mode),
		sys_le32_to_cpu(master_rsp->mode),
		"mode: expected: %x actual: %x",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, mode),
		sys_le32_to_cpu(master_rsp->mode)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, flags),
		sys_le32_to_cpu(master_rsp->flags),
		"flags: expected: %x actual: %x",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0, flags),
		sys_le32_to_cpu(master_rsp->flags)
	);
}

void test_greybus_spi_device_config(void)
{
	uint8_t req_[
		0
		+ sizeof(struct gb_operation_hdr)
		+ sizeof(struct gb_spi_device_config_request)
	] = {};
	struct gb_operation_hdr *const req =
		(struct gb_operation_hdr *)req_;

	req->size = sys_cpu_to_le16(sizeof(req_));
	req->id = sys_cpu_to_le16(0xabcd);
	req->type = GB_SPI_TYPE_DEVICE_CONFIG;

	uint8_t rsp_[0 + sizeof(struct gb_operation_hdr) +
		     sizeof(struct gb_spi_device_config_response)] = {};

	struct gb_operation_hdr *const rsp =
		(struct gb_operation_hdr *)rsp_;

	const size_t rsp_size = sizeof(rsp_);
	struct gb_spi_device_config_response *const device_rsp =
		(struct gb_spi_device_config_response
			 *)(rsp_ + sizeof(struct gb_operation_hdr));

	tx_rx(req, rsp, rsp_size);

	zassert_equal(rsp->result, GB_OP_SUCCESS,
		      "expected: GB_OP_SUCCESS actual: %u",
		      rsp->result);

	/*
	 * Zephyr currently has no way to query these settings from the hardware
	 * but at least they are compile-time constant and can be extracted out
	 * of devicetree
	 */
	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, mode),
		sys_le32_to_cpu(device_rsp->mode),
		"mode: expected: %x actual: %x",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, mode),
		sys_le32_to_cpu(device_rsp->mode)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, bpw),
		sys_le32_to_cpu(device_rsp->bpw),
		"bpw: expected: %u actual: %u",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, bpw),
		sys_le32_to_cpu(device_rsp->bpw)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, max_speed_hz),
		sys_le32_to_cpu(device_rsp->max_speed_hz),
		"max_speed_hz: expected: %u actual: %u",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, max_speed_hz),
		sys_le32_to_cpu(device_rsp->max_speed_hz)
	);

	zassert_equal(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, device_type),
		sys_le32_to_cpu(device_rsp->device_type),
		"device_type: expected: %u actual: %u",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, device_type),
		sys_le32_to_cpu(device_rsp->device_type)
	);

	zassert_equal(
		0, strcmp(
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, device_name),
		sys_le32_to_cpu(device_rsp->name)),
		"device_type: expected: %u actual: %u",
		DT_PROP(DT_N_S_greybus0_S_gbbundle1_S_gbspi0_S_eeprom, device_name),
		sys_le32_to_cpu(device_rsp->name)
	);
}

void test_greybus_spi_transfer(void)
{
	/*
	 * This is from test_zero_length_write() in tests/drivers/spi/spi_eeprom
	 * adapted from tests/drivers/eeprom.
	 *
	 * D: spi_sim_callback(): RDSR  len: 2 data: [05, 00]
	 * D: spi_sim_callback(): WREN  len: 1 data: [06]
	 * D: spi_sim_callback(): WRITE len: 4 data: [10, 20, 30, 40] addr: 000000
	 * D: spi_sim_callback(): RDSR  len: 2 data: [05, 00]
	 * D: spi_sim_callback(): READ  len: 4 data: [10, 20, 30, 40] addr: 000000
	 */

#define DESC(_speed, _len, _delay, _cs_change, _bpw, _rdwr) \
	{ \
		.speed_hz = sys_cpu_to_le32(_speed), \
		.len = sys_cpu_to_le32(_len), \
		.delay_usecs = sys_cpu_to_le16(_delay), \
		.cs_change = _cs_change, \
		.bits_per_word = _bpw, \
		.rdwr = _rdwr, \
	}

	static const struct gb_spi_transfer_desc descs[] = {
		/* RDSR  */ DESC(100000, 2, 0, 0, 8, GB_SPI_XFER_WRITE | GB_SPI_XFER_READ),
		/* WREN  */ DESC(100000, 1, 0, 0, 8, GB_SPI_XFER_WRITE),
		/* WRITE */ DESC(100000, 8, 0, 0, 8, GB_SPI_XFER_WRITE),
		/* RDSR  */ DESC(100000, 2, 0, 0, 8, GB_SPI_XFER_WRITE | GB_SPI_XFER_READ),
		/* READ  */ DESC(100000, 8, 0, 0, 8, GB_SPI_XFER_WRITE | GB_SPI_XFER_READ),
	};

	uint8_t req_[
		0
		+ sizeof(struct gb_operation_hdr)
		+ sizeof(struct gb_spi_transfer_request)
		+ ARRAY_SIZE(descs) * sizeof(struct gb_spi_transfer_desc)
		/*
		 * SPI writes also include 0s for clocking-out reads. So
		 * the Greybus SPI request is slightly different than the
		 * I2C request.
		 */
		+ (2 + 1 + (4 + 4) + 2 + (4 + 4))
		] = {};
	const size_t req_size = sizeof(req_);

	/* AT25 instruction set */
	enum {
		AT25_WRSR = 1, /* Write STATUS register        */
		AT25_WRITE = 2, /* Write data to memory array   */
		AT25_READ = 3, /* Read data from memory array  */
		AT25_WRDI = 4, /* Reset the write enable latch */
		AT25_RDSR = 5, /* Read STATUS register         */
		AT25_WREN = 6, /* Set the write enable latch   */
		AT25_EONE = 0x52, /* Erase One Sector in Memory Array */
		AT25_EALL = 0x62, /* Erase All Sectors in Memory Array */
		AT25_RDR = 0x15, /* Read Manufacturer and Product ID */
	};

	static const uint8_t write_data[] = {
		AT25_RDSR,
			/* 1 byte of clocked-out for read */
			0x00,
		AT25_WREN,
		AT25_WRITE,
			/* 3-byte address */
			0x00, 0x00, 0x00,
			/* 4 bytes of write data */
			0x10, 0x20, 0x30, 0x40,
		AT25_RDSR,
			/* 1 byte of clocked-out for read */
			0x00,
		AT25_READ,
			/* 3-byte address */
			0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, /* 4 bytes of clocked-out for read */
	};

	struct gb_operation_hdr *const req =
		(struct gb_operation_hdr *)req_;
	
	req->size = sys_cpu_to_le16(req_size);
	req->id = sys_cpu_to_le16(0xabcd);
	req->type = GB_SPI_PROTOCOL_TRANSFER;

	struct gb_spi_transfer_request *const xfer_req =
		(struct gb_spi_transfer_request *)
		(req_ + sizeof(*req));
	xfer_req->count = sys_cpu_to_le16(ARRAY_SIZE(descs));

	uint8_t rsp_[
		0
		+ sizeof(struct gb_operation_hdr)
		+ (12 /* response size only counts read ops */)
		];
	const size_t rsp_size = sizeof(rsp_);
	struct gb_operation_hdr *const rsp =
		(struct gb_operation_hdr *)rsp_;

	struct gb_spi_transfer_response *const xfer_rsp =
		(struct gb_spi_transfer_response *)
		(rsp_ + sizeof(struct gb_operation_hdr));

	const uint8_t expected_data[] = {
		AT25_RDSR, 0x00,
		AT25_RDSR, 0x02,
		AT25_READ, 0x00, 0x00, 0x00, 0x10, 0x20, 0x30, 0x40,
	};
	uint8_t *const actual_data = xfer_rsp->data;
	memset(actual_data, 0xff, 12);

	uint8_t *payload = (uint8_t *)xfer_req->transfers;
	memcpy(payload, descs, sizeof(descs));
	payload += sizeof(descs);
	memcpy(payload, write_data, sizeof(write_data)); 

	tx_rx(req, rsp, rsp_size);

	zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
		      "expected: GB_OP_SUCCESS actual: %u",
		      ((struct gb_operation_hdr *)rsp_)->result);


	zassert_equal(
		0,
		memcmp(expected_data, actual_data, sizeof(expected_data)),
		"data: expected: [%s] actual: [%s]",
		to_string((uint8_t *)expected_data, sizeof(expected_data)),
		to_string(actual_data, sizeof(expected_data))
	);
}
