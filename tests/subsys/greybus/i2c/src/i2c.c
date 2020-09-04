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

#undef perror
#define perror(s) LOG_ERR("%s", s)

#endif

#include "../../../../subsys/greybus/i2c-gb.h"

#include "test-greybus-i2c.h"

#define TIMEOUT_MS 1000
#define PORT 4243

static struct device *i2c_dev;

static int fd = -1;

void test_greybus_setup(void)
{
	struct sockaddr_in6 addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = in6addr_loopback,
		.sin6_port = htons(PORT),
	};
	socklen_t addrlen = sizeof(addr);
	int r;

	i2c_dev = device_get_binding(I2C_DEV_NAME);
	zassert_not_equal(i2c_dev, NULL,
			  "failed to get device binding for " I2C_DEV_NAME);

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
	const size_t hdr_size = sizeof(struct gb_operation_hdr);

	size = sys_le16_to_cpu(req->size);
	r = send(fd, req, size, 0);
	zassert_not_equal(r, -1, "send: %s", errno);
	zassert_equal(r, size, "write: expected: %d actual: %d", size, r);

	pollfd.fd = fd;
	pollfd.events = POLLIN;

	r = poll(&pollfd, 1, TIMEOUT_MS);
	zassert_not_equal(r, -1, "poll: %s", errno);
	zassert_not_equal(r, 0, "timeout waiting for response");
	zassert_equal(r, 1, "invalid number of pollfds with data: %d", r);

	r = recv(fd, rsp, hdr_size, 0);
	zassert_not_equal(r, -1, "recv: %s", errno);
	zassert_equal(hdr_size, r, "recv: expected: %u actual: %u",
		      (unsigned)hdr_size, r);

	zassert_equal(rsp->id, req->id,
			  "expected: 0x%04x actual: 0x%04x",
			  sys_le16_to_cpu(req->id),
			  sys_le16_to_cpu(rsp->id));
	zassert_equal(rsp->type, GB_TYPE_RESPONSE_FLAG | req->type,
			  "expected: %u actual: %u",
			  GB_TYPE_RESPONSE_FLAG | req->type, rsp->type);
}

void test_greybus_i2c_protocol_version(void)
{
	const struct gb_operation_hdr req = {
		.size = sys_cpu_to_le16(sizeof(struct gb_operation_hdr)),
		.id = sys_cpu_to_le16(0xabcd),
		.type = GB_I2C_PROTOCOL_VERSION,
	};
	uint8_t rsp_[0 + sizeof(struct gb_operation_hdr) +
		     sizeof(struct gb_i2c_proto_version_response)];
	const size_t rsp_size = sizeof(rsp_);
	struct gb_i2c_proto_version_response *const rsp =
		(struct gb_i2c_proto_version_response
			 *)(rsp_ + sizeof(struct gb_operation_hdr));

	tx_rx(&req, (struct gb_operation_hdr *)rsp_, rsp_size);

	zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
		      "expected: GB_OP_SUCCESS actual: %u",
		      ((struct gb_operation_hdr *)rsp_)->result);

	/* GB_I2C_VERSION_MAJOR (0) is buried in subsys/greybus/i2c.c */
	zassert_equal(rsp->major, 0, "expected: %u actual: %u", 0, rsp->major);

	/* GB_I2C_VERSION_MINOR (1) is buried in subsys/greybus/i2c.c */
	zassert_equal(rsp->minor, 1, "expected: %u actual: %u", 1, rsp->minor);
}

void test_greybus_i2c_cport_shutdown(void)
{
	/*
	 * looks like the original NuttX implementation was missing any kind
	 * of cport shutdown request
	 */
}

void test_greybus_i2c_functionality(void)
{
	const struct gb_operation_hdr req = {
		.size = sys_cpu_to_le16(sizeof(struct gb_operation_hdr)),
		.id = sys_cpu_to_le16(0xabcd),
		.type = GB_I2C_PROTOCOL_FUNCTIONALITY,
	};
	uint8_t rsp_[0 + sizeof(struct gb_operation_hdr) +
		     sizeof(struct gb_i2c_functionality_rsp)];
	const size_t rsp_size = sizeof(rsp_);
	struct gb_i2c_functionality_rsp *const rsp =
		(struct gb_i2c_functionality_rsp
			 *)(rsp_ + sizeof(struct gb_operation_hdr));
	uint32_t expected_uint32;
	uint32_t actual_uint32;

	tx_rx(&req, (struct gb_operation_hdr *)rsp_, rsp_size);

	zassert_equal(((struct gb_operation_hdr *)rsp_)->result, GB_OP_SUCCESS,
		      "expected: GB_OP_SUCCESS actual: %u",
		      ((struct gb_operation_hdr *)rsp_)->result);

	expected_uint32 = GB_I2C_FUNC_I2C | GB_I2C_FUNC_SMBUS_READ_BYTE |
			  GB_I2C_FUNC_SMBUS_WRITE_BYTE |
			  GB_I2C_FUNC_SMBUS_READ_BYTE_DATA |
			  GB_I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
			  GB_I2C_FUNC_SMBUS_READ_WORD_DATA |
			  GB_I2C_FUNC_SMBUS_WRITE_WORD_DATA |
			  GB_I2C_FUNC_SMBUS_READ_I2C_BLOCK |
			  GB_I2C_FUNC_SMBUS_WRITE_I2C_BLOCK;

	actual_uint32 = sys_le32_to_cpu(rsp->functionality);

	zassert_equal(expected_uint32, actual_uint32,
		      "functionality: expected: %08x actual: %08x",
		      expected_uint32, actual_uint32);
}

void test_greybus_i2c_transfer(void)
{
}
