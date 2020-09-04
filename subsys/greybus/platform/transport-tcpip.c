#include <errno.h>
#include <net/dns_sd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr.h>
#include <sys/dlist.h>

#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) \
    || defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) \
    || defined(CONFIG_BOARD_NRF52_BSIM)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/byteorder.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

/* For some reason, including <net/net_ip.h> breaks everything
 * I only need these */
static inline struct sockaddr_in *net_sin(struct sockaddr *sa)
{
	return (struct sockaddr_in *)sa;
}

static inline struct sockaddr_in6 *net_sin6(struct sockaddr *sa)
{
	return (struct sockaddr_in6 *)sa;
}

extern int usleep(useconds_t usec);

#else

#include <greybus/debug.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <greybus-utils/manifest.h>
#include <posix/unistd.h>
#include <posix/pthread.h>
#include <net/net_ip.h>

unsigned int sleep(unsigned int seconds)
{
	k_sleep(K_MSEC(1000 * seconds));
	return 0;
}

int usleep(useconds_t usec) {
	k_usleep(usec);
	return 0;
}

#endif

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_transport_tcpip, LOG_LEVEL_INF);

#include "transport.h"

/* Based on UniPro, from Linux */
#define CPORT_ID_MAX 4095

#define GB_TRANSPORT_TCPIP_BASE_PORT 4242
#define GB_TRANSPORT_TCPIP_BACKLOG 10

enum fd_context_type {
	FD_CONTEXT_SERVER = 1,
	FD_CONTEXT_CLIENT = 2,
	FD_CONTEXT_ANY = 3,
};

struct fd_context {
	sys_dnode_t node;
    int fd;
    int cport;
    bool listen;
    enum fd_context_type type;
};

DNS_SD_REGISTER_TCP_SERVICE(gb_service_advertisement, CONFIG_NET_HOSTNAME,
	"_greybus", "local", DNS_SD_EMPTY_TXT, GB_TRANSPORT_TCPIP_BASE_PORT);

static sys_dlist_t fd_list;
static pthread_mutex_t fd_list_mutex;
static pthread_t accept_thread;

static struct fd_context *fd_context_new(int fd, int cport, enum fd_context_type type)
{
	struct fd_context *ctx = NULL;

	if (fd < 0) {
		LOG_ERR("invalid fd %d", fd);
		return NULL;
	}

	if (cport < 0 || cport > CPORT_ID_MAX) {
		LOG_ERR("invalid cport %d", cport);
		return NULL;
	}

	if (!(type == FD_CONTEXT_SERVER || type == FD_CONTEXT_CLIENT)) {
		LOG_ERR("invalid type %u", type);
		return NULL;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		LOG_ERR("failed to allocate context");
		return NULL;
	}

	ctx->fd = fd;
	ctx->cport = cport;
	ctx->type = type;
	sys_dnode_init(&ctx->node);

	return ctx;
}

static void fd_context_delete(struct fd_context *ctx)
{
	if (ctx == NULL) {
		return;
	}

	close(ctx->fd);
	free(ctx);
}

static bool fd_context_insert(int fd, int cport, enum fd_context_type type)
{
	struct fd_context *ctx;
	struct fd_context *cnode;
	int r;
	bool success = false;

	ctx = fd_context_new(fd, cport, type);
	if (ctx == NULL) {
		goto out;
	}

	r = pthread_mutex_lock(&fd_list_mutex);
	if (r != 0) {
		LOG_ERR("failed to lock fd_list_mutex (%d)", r);
		goto out;
	}

	SYS_DLIST_FOR_EACH_CONTAINER(&fd_list, cnode, node) {
		/* check for uniqueness on the fd key */
		if (cnode->fd == ctx->fd) {
			LOG_ERR("fd_list already contains fd %d", ctx->fd);
			goto unlock;
		}
	}

	sys_dlist_append(&fd_list, &ctx->node);
	success = true;

unlock:
	pthread_mutex_unlock(&fd_list_mutex);
out:
	if (!success) {
		free(ctx);
	}
	return success;
}

static void fd_context_erase_inner(struct fd_context *ctx)
{
	if (ctx == NULL) {
		return;
	}
	sys_dlist_remove(&ctx->node);
	fd_context_delete(ctx);
}

static bool fd_context_erase(int fd)
{
	struct fd_context *cnode;
	int r;
	bool success = false;
	struct fd_context *ctx = NULL;

	r = pthread_mutex_lock(&fd_list_mutex);
	if (r != 0) {
		LOG_ERR("failed to lock fd_list_mutex (%d)", r);
		goto out;
	}

	SYS_DLIST_FOR_EACH_CONTAINER(&fd_list, cnode, node) {
		if (cnode->fd == fd) {
			ctx = cnode;
			break;
		}
	}

	if (ctx == NULL) {
		LOG_DBG("fd %d is not in list", fd);
		goto unlock;
	}

	fd_context_erase_inner(ctx);
	success = true;

unlock:
	pthread_mutex_unlock(&fd_list_mutex);
out:
	return success;
}

static void fd_context_clear(void)
{
	int r;
	struct fd_context *ctx;
	struct fd_context *cnode;

	r = pthread_mutex_lock(&fd_list_mutex);
	if (r != 0) {
		LOG_ERR("failed to lock fd_list_mutex (%d)", r);
		return;
	}

	for ( ctx = SYS_DLIST_PEEK_HEAD_CONTAINER(&fd_list, cnode, node);
		ctx != NULL; ctx = SYS_DLIST_PEEK_HEAD_CONTAINER(&fd_list, cnode, node)) {
		fd_context_erase_inner(ctx);
	}

	pthread_mutex_unlock(&fd_list_mutex);
}

static struct fd_context *fd_context_find(int fd, int cport, enum fd_context_type type)
{
	struct fd_context *cnode;
	int r;
	struct fd_context *ctx = NULL;

	r = pthread_mutex_lock(&fd_list_mutex);
	if (r != 0) {
		LOG_ERR("failed to lock fd_list_mutex (%d)", r);
		goto out;
	}

	SYS_DLIST_FOR_EACH_CONTAINER(&fd_list, cnode, node) {
		if (!(fd == -1 || fd == cnode->fd)) {
			continue;
		}

		if (!(cport == -1 || cport == cnode->cport)) {
			continue;
		}

		if ((type & cnode->type) == 0) {
			continue;
		}

		ctx = cnode;
		break;
	}

	pthread_mutex_unlock(&fd_list_mutex);

out:
	return ctx;
}

static inline struct fd_context *fd_to_context(int fd)
{
	return fd_context_find(fd, -1, FD_CONTEXT_ANY);
}

static inline struct fd_context *cport_to_server_context(int cport)
{
	return fd_context_find(-1, cport, FD_CONTEXT_SERVER);
}

static int getMessage(int fd, struct gb_operation_hdr **msg);
static int sendMessage(int fd, struct gb_operation_hdr *msg);

static void accept_new_connection(struct fd_context *ctx)
{
	int fd;
	struct sockaddr_in6 addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = in6addr_any,
	};
	socklen_t addrlen;
	static char addrstr[INET6_ADDRSTRLEN];
	char *addrstrp;

	__ASSERT_NO_MSG(ctx->type == FD_CONTEXT_SERVER);

    addrlen = sizeof(addr);
    fd = accept(ctx->fd, (struct sockaddr *)&addr, &addrlen);
    if (fd == -1) {
        LOG_ERR("accept: %d", errno);
        return;
    }

    memset(addrstr, '\0', sizeof(addrstr));
    addrstrp = (char *)inet_ntop(addr.sin6_family, &addr.sin6_addr,
        addrstr, sizeof(addrstr));
    if (NULL == addrstrp) {
        LOG_ERR("inet_ntop: %d", errno);
        return;
    }

    if (!fd_context_insert(fd, ctx->cport, FD_CONTEXT_CLIENT)) {
    	close(fd);
    	return;
    }

    LOG_DBG("cport %d accepted connection from [%s]:%d as fd %d",
        ctx->cport, log_strdup(addrstr), ntohs(addr.sin6_port), fd);
}

static void handle_client_input(struct fd_context *ctx)
{
	int r;
	struct gb_operation_hdr *msg = NULL;

	r = getMessage(ctx->fd, &msg);
	if (r == 0) {
		/* Connection was shut down gracefully */
		goto close_conn;
	}

	if (r < 0) {
		LOG_ERR("fd %d returned %d", ctx->fd, r);
		goto close_conn;
	}

	r = greybus_rx_handler(ctx->cport, msg, sys_le16_to_cpu(msg->size));
	if (r == 0) {
		/* Message handled properly */
		goto free_msg;
	}

	__ASSERT_NO_MSG(r < 0);

	LOG_ERR("cport %u failed to handle message: size: %u, id: %u, type: %u",
		ctx->cport, sys_le16_to_cpu(msg->size), sys_le16_to_cpu(msg->id),
		msg->type);

close_conn:
	LOG_DBG("closing fd %d", ctx->fd);
	fd_context_erase(ctx->fd);

free_msg:
	free(msg);
}

/* return the number of valid entries in the pollfds array */
static int prepare_pollfds(struct pollfd *pollfds, size_t array_size)
{
	int r;
    struct fd_context *cnode;
    sys_dnode_t *node;
    size_t fds;

    memset(pollfds, 0, array_size * sizeof(*pollfds));

	r = pthread_mutex_lock(&fd_list_mutex);
	if (r != 0) {
		LOG_ERR("failed to lock fd_list_mutex (%d)", r);
		r = -r;
		goto out;
	}

	fds = 0;
	SYS_DLIST_FOR_EACH_NODE(&fd_list, node) {
		++fds;
	}

	if (fds > array_size) {
		LOG_ERR("Number of fds (%zu) exceeds number of pollfds available (%zu)", fds, array_size);
		r = -E2BIG;
		goto unlock;
	}

	r = 0;
	SYS_DLIST_FOR_EACH_CONTAINER(&fd_list, cnode, node) {
		switch(cnode->type) {
		case FD_CONTEXT_SERVER:
		case FD_CONTEXT_CLIENT:
		default:
			pollfds[r].fd = cnode->fd;
			pollfds[r].events = POLLIN;
			r++;
			break;
		}
	}

	r = fds;

unlock:
    pthread_mutex_unlock(&fd_list_mutex);
out:
    return r;
}

static void *service_thread(void *arg)
{
	int r;
    struct fd_context *ctx;
    int pollfds_size;
    static struct pollfd pollfds[CONFIG_NET_SOCKETS_POLL_MAX];

	for (;;) {
		pollfds_size = prepare_pollfds(pollfds, ARRAY_SIZE(pollfds));
		if (pollfds_size <= 0) {
			LOG_DBG("prepare_pollfds() returned %d", pollfds_size);
			break;
		}

		r = poll(pollfds, pollfds_size, -1);
		if (-1 == r) {
			LOG_ERR("poll failed: %d", errno);
			break;
		}

		for(size_t i = 0, revents = r; revents > 0 && i < pollfds_size; ++i) {
			if (pollfds[i].revents & POLLIN) {
				ctx = fd_to_context(pollfds[i].fd);

				if (ctx == NULL) {
					LOG_DBG("ctx is NULL");
				} else {
					switch(ctx->type) {
					case FD_CONTEXT_SERVER:
						accept_new_connection(ctx);
						break;
					case FD_CONTEXT_CLIENT:
						handle_client_input(ctx);
						break;
					default:
						LOG_ERR("ctx@%p has invalid type %u", ctx, ctx->type);
						break;
					}
				}

				--revents;
			}
		}
	}

	LOG_WRN("Greybus is quitting");
	fd_context_clear();

	return NULL;
}

static int getMessage(int fd, struct gb_operation_hdr **msg)
{
	int r;
	void *tmp;
	size_t msg_size;
	size_t payload_size;
	size_t remaining;
	size_t offset;
	size_t recvd;

	if (NULL == msg) {
		LOG_DBG("One or more arguments were NULL or invalid");
		return -EINVAL;
	}

	tmp = realloc(*msg, sizeof(**msg));
	if (NULL == tmp) {
		LOG_DBG("Failed to allocate memory");
		return -ENOMEM;
	}

	*msg = tmp;

	for (remaining = sizeof(**msg), offset = 0, recvd = 0; remaining;
	     remaining -= recvd, offset += recvd, recvd = 0) {

		r = recv(fd, &((uint8_t *)*msg)[offset], remaining, 0);
		if (r == 0) {
			/* Connection shut down gracefully */
			return 0;
		}

		if (r == -1) {
			return -errno;
		}

		recvd = r;
	}

	msg_size = sys_le16_to_cpu((*msg)->size);
	if (msg_size < sizeof(struct gb_operation_hdr)) {
		LOG_DBG("invalid message size %u", (unsigned)msg_size);
		return -EINVAL;
	}

	payload_size = msg_size - sizeof(**msg);
	if (payload_size > GB_MAX_PAYLOAD_SIZE) {
		LOG_DBG("invalid payload size %u", (unsigned)payload_size);
		return -EINVAL;
	}

	if (payload_size > 0) {
		tmp = realloc(*msg, msg_size);
		if (NULL == tmp) {
			LOG_DBG("Failed to allocate memory");
			return -ENOMEM;
		}

		*msg = tmp;

		for (remaining = payload_size, offset = sizeof(**msg),
		    recvd = 0;
		     remaining;
		     remaining -= recvd, offset += recvd, recvd = 0) {

			r = recv(fd, &((uint8_t *)*msg)[offset], remaining, 0);
			if (r == 0) {
				/* Connection shut down gracefully */
				return 0;
			}

			if (r < 0) {
				return -errno;
			}

			recvd = r;
		}
	}

	return msg_size;
}

static int sendMessage(int fd, struct gb_operation_hdr *msg)
{
	int r;
	size_t offset;
	size_t remaining;
	size_t written;

	for (remaining = sys_le16_to_cpu(msg->size), offset = 0, written = 0;
	     remaining; remaining -= written, offset += written, written = 0) {

		r = send(fd, &((uint8_t *)msg)[offset], remaining, 0);

		if (r < 0) {
			LOG_ERR("send: %d", errno);
			return -errno;
		}

		if (0 == r) {
			LOG_ERR("send returned 0 - EOF?");
			return -ENOTCONN;
		}

		written = r;
	}

	return 0;
}

static void gb_xport_init(void)
{
	LOG_DBG("");
}
static void gb_xport_exit(void)
{
	LOG_DBG("");
}

static int gb_xport_listen_start(unsigned int cport)
{
	LOG_DBG("cport %d", cport);
	return 0;
}

static int gb_xport_listen__stop(unsigned int cport)
{
	LOG_DBG("cport %d", cport);
	return 0;
}

static int gb_xport_send(unsigned int cport, const void *buf, size_t len)
{
	int r;
	struct gb_operation_hdr *msg;
	struct fd_context *ctx;

	msg = (struct gb_operation_hdr *)buf;
    if (NULL == msg) {
		LOG_ERR("message is NULL");
	    return -EINVAL;
	}

    if (sys_le16_to_cpu(msg->size) != len || len < sizeof(*msg)) {
		LOG_ERR("invalid message size %u (len: %u)",
			(unsigned)sys_le16_to_cpu(msg->size), (unsigned)len);
        return -EINVAL;
    }

    ctx = fd_context_find(-1, cport, FD_CONTEXT_CLIENT);
    if (ctx == NULL) {
    	LOG_ERR("failed to find client fd_context for cport %d", cport);
    	return -EINVAL;
    }

    r = sendMessage(ctx->fd, msg);
    if (r != 0) {
    	fd_context_erase(ctx->fd);
    }

    return r;
}

static void *gb_xport_alloc_buf(size_t size)
{
	void *p = malloc(size);

	if (!p) {
		LOG_ERR("Failed to allocate %zu bytes", size);
	}

	return p;
}

static void gb_xport_free__buf(void *ptr)
{
	free(ptr);
}

static const struct gb_transport_backend gb_xport = {
	.init = gb_xport_init,
	.exit = gb_xport_exit,
	.listen = gb_xport_listen_start,
	.stop_listening = gb_xport_listen__stop,
	.send = gb_xport_send,
	.send_async = NULL,
	.alloc_buf = gb_xport_alloc_buf,
	.free_buf = gb_xport_free__buf,
};

static int netsetup(int *cports, size_t num_cports)
{
	int r;
	int fd;
    size_t i;
	const int yes = true;
	int family;
	uint16_t *port;
	struct sockaddr sa;
	socklen_t sa_len;

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		family = AF_INET6;
		net_sin6(&sa)->sin6_family = AF_INET6;
		net_sin6(&sa)->sin6_addr = in6addr_any;
		port = &net_sin6(&sa)->sin6_port;
		sa_len = sizeof(struct sockaddr_in6);
	} else if (IS_ENABLED(CONFIG_NET_IPV4)) {
		family = AF_INET;
		net_sin(&sa)->sin_family = AF_INET;
		net_sin(&sa)->sin_addr.s_addr = INADDR_ANY;
		port = &net_sin(&sa)->sin_port;
		sa_len = sizeof(struct sockaddr_in);
	} else {
		LOG_ERR("Neither IPv6 nor IPv4 is available");
		return -EINVAL;
	}
	*port = htons(GB_TRANSPORT_TCPIP_BASE_PORT);

    for(i = 0; i < num_cports; ++i) {
        fd = socket(family, SOCK_STREAM, 0);
        if (fd == -1) {
            LOG_ERR("socket: %d", errno);
            return -errno;
        }

        if (!fd_context_insert(fd, cports[i], FD_CONTEXT_SERVER)) {
        	LOG_ERR("failed to add fd context for cport %d", cports[i]);
        	close(fd);
        	return -EINVAL;
        }

        r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (-1 == r) {
            LOG_ERR("setsockopt: %d", errno);
            return -errno;
        }

    	*port = htons(GB_TRANSPORT_TCPIP_BASE_PORT + i);
        r = bind(fd, &sa, sa_len);
        if (-1 == r) {
            LOG_ERR("bind: %d", errno);
            return -errno;
        }

        r = listen(fd, GB_TRANSPORT_TCPIP_BACKLOG);
        if (-1 == r) {
            LOG_ERR("listen: %d", errno);
            return -errno;
        }

        LOG_INF("CPort %d mapped to TCP/IP port %u",
			cports[i], GB_TRANSPORT_TCPIP_BASE_PORT + i);
    }

    return 0;
}

struct gb_transport_backend *gb_transport_backend_init(unsigned int *cports, size_t num_cports) {

    int r;
    struct gb_transport_backend *ret = NULL;

	LOG_DBG("Greybus TCP/IP Transport initializing..");

	pthread_mutex_init(&fd_list_mutex, NULL);
	sys_dlist_init(&fd_list);
    if (num_cports >= CPORT_ID_MAX) {
        LOG_ERR("invalid number of cports %u", (unsigned)num_cports);
        goto out;
    }

    r = netsetup(cports, num_cports);
    if (r < 0) {
    	LOG_ERR("netsetup() failed: %d", r);
        goto cleanup;
    }

	r = pthread_create(&accept_thread, NULL, service_thread, NULL);
	if (r != 0) {
		LOG_ERR("pthread_create: %d", r);
		goto cleanup;
	}

	(void)pthread_setname_np(accept_thread, "greybus");

    ret = (struct gb_transport_backend *)&gb_xport;

	LOG_INF("Greybus TCP/IP Transport initialized");
	goto out;

cleanup:
	fd_context_clear();

out:
    return ret;
}
