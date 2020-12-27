/*
 * Copyright (c) 2014-2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */
#include <zephyr.h>

#include <list.h>
#include <unipro/unipro.h>
#include <greybus/greybus.h>
#include <greybus/tape.h>
//#include <wdog.h>
#include "greybus-stubs.h"
//#include <loopback-gb.h>
#include <logging/log.h>

#include <greybus-utils/manifest.h>

#include <sys/atomic.h>
#include <sys/byteorder.h>
#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) \
    || defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) \
    || defined(CONFIG_BOARD_NRF52_BSIM)
#include <pthread.h>
#include <semaphore.h>
/* For some reason, not declared even with _GNU_SOURCE */
extern int pthread_setname_np(pthread_t thread, const char *name);

#define DEFAULT_STACK_SIZE PTHREAD_STACK_MIN

#else

#include <posix/pthread.h>
#include <posix/semaphore.h>

#define DEFAULT_STACK_SIZE      CONFIG_PTHREAD_DYNAMIC_STACK_DEFAULT_SIZE

void qsort(void *base, size_t nmemb, size_t size,
                  int (*compar)(const void *, const void *));

#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(greybus, CONFIG_GREYBUS_LOG_LEVEL);

#if !defined(CONFIG_POSIX_API)
/*
 * Currently CONFIG_POSIX_API is incompatible with
 * CONFIG_NET_SOCKETS_POSIX_NAMES
 */
#define clock_gettime _m_clock_gettime
static int _m_clock_gettime(clockid_t clk_id, struct timespec *tp) {
	ARG_UNUSED(clk_id);

	uint64_t uptime_ms = k_uptime_get();
	tp->tv_sec = uptime_ms / 1000;
	tp->tv_nsec = (uptime_ms % 1000) * 1000000;

	return 0;
}
#endif

#define TIMEOUT_IN_MS           1000
#define GB_PING_TYPE            0x00

#define ONE_SEC_IN_MSEC         1000
#define ONE_MSEC_IN_NSEC        1000000

#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 100
#endif

#define TIMEOUT_WD_DELAY    (TIMEOUT_IN_MS * CLOCKS_PER_SEC) / ONE_SEC_IN_MSEC

#define DEBUGASSERT(x)
#define atomic_init(ptr, val) *(ptr) = val

struct wdog_s {
	int woof;
};
struct gb_cport_driver {
    struct gb_driver *driver;
    struct list_head tx_fifo;
    struct list_head rx_fifo;
    sem_t rx_fifo_lock;
    pthread_t thread;
    volatile bool exit_worker;
    struct wdog_s timeout_wd;
    struct gb_operation timedout_operation;
};

struct gb_tape_record_header {
    uint16_t size;
    uint16_t cport;
};

static unsigned int cport_count;
static atomic_t request_id;
static struct gb_cport_driver *g_cport;
static struct gb_bundle **g_bundle;
static struct gb_transport_backend *transport_backend;
static struct gb_tape_mechanism *gb_tape;
static int gb_tape_fd = -EBADF;
static struct gb_operation_hdr timedout_hdr = {
    .size = sizeof(timedout_hdr),
    .result = GB_OP_TIMEOUT,
    .type = GB_TYPE_RESPONSE_FLAG,
};
static struct gb_operation_hdr oom_hdr = {
    .size = sizeof(timedout_hdr),
    .result = GB_OP_NO_MEMORY,
    .type = GB_TYPE_RESPONSE_FLAG,
};

static void gb_operation_timeout(int argc, uint32_t cport, ...);
static struct gb_operation *_gb_operation_create(unsigned int cport);

uint8_t gb_errno_to_op_result(int err)
{
    switch (err) {
    case 0:
        return GB_OP_SUCCESS;

    case ENOMEM:
    case -ENOMEM:
        return GB_OP_NO_MEMORY;

    case EINTR:
    case -EINTR:
        return GB_OP_INTERRUPTED;

    case ETIMEDOUT:
    case -ETIMEDOUT:
        return GB_OP_TIMEOUT;

    case EPROTO:
    case -EPROTO:
    case ENOSYS:
    case -ENOSYS:
        return GB_OP_PROTOCOL_BAD;

    case EINVAL:
    case -EINVAL:
        return GB_OP_INVALID;

#ifndef EOVERFLOW
#define EOVERFLOW 75
#endif

    case EOVERFLOW:
    case -EOVERFLOW:
        return GB_OP_OVERFLOW;

    case ENODEV:
    case -ENODEV:
    case ENXIO:
    case -ENXIO:
        return GB_OP_NONEXISTENT;

    case EBUSY:
    case -EBUSY:
        return GB_OP_RETRY;

    default:
        return GB_OP_UNKNOWN_ERROR;
    }
}

#ifdef CONFIG_GREYBUS_FEATURE_HAVE_TIMESTAMPS
static void op_mark_send_time(struct gb_operation *operation)
{
    clock_gettime(CLOCK_REALTIME, &operation->send_ts);
}

static void op_mark_recv_time(struct gb_operation *operation)
{
    clock_gettime(CLOCK_REALTIME, &operation->recv_ts);
}
#else
static void op_mark_send_time(struct gb_operation *operation) { }
static void op_mark_recv_time(struct gb_operation *operation) { }
#endif

static int gb_compare_handlers(const void *data1, const void *data2)
{
    const struct gb_operation_handler *handler1 = data1;
    const struct gb_operation_handler *handler2 = data2;
    return (int)handler1->type - (int)handler2->type;
}

static struct gb_operation_handler *find_operation_handler(uint8_t type,
                                                           unsigned int cport)
{
    struct gb_driver *driver = g_cport[cport].driver;
    int l,r;

    if (type == GB_INVALID_TYPE || !driver->op_handlers) {
        return NULL;
    }

    /*
     * This function is performance sensitive, so let's use an inline binary
     * search algorithm. The libc version takes pointer to the comparison
     * function as argument which is then called via full-blown function
     * calls. The below version doesn't require calling any other function.
     */
    l = 0;
    r = driver->op_handlers_count - 1;
    while (l <= r) {
        int m = (l + r) / 2;
        if (driver->op_handlers[m].type < type)
            l = m + 1;
        else if (driver->op_handlers[m].type > type)
            r = m - 1;
        else
            return &driver->op_handlers[m];
    }

    return NULL;
}

static void gb_process_request(struct gb_operation_hdr *hdr,
                               struct gb_operation *operation)
{
    struct gb_operation_handler *op_handler;
    uint8_t result;

    if (hdr->type == GB_PING_TYPE) {
        gb_operation_send_response(operation, GB_OP_SUCCESS);
        return;
    }

    op_handler = find_operation_handler(hdr->type, operation->cport);
    if (!op_handler) {
        LOG_ERR("Cport %u: Invalid operation type %u",
                 operation->cport, hdr->type);
        gb_operation_send_response(operation, GB_OP_INVALID);
        return;
    }

    operation->bundle = g_cport[operation->cport].driver->bundle;

    result = op_handler->handler(operation);
    LOG_DBG("%s: %u", log_strdup(gb_handler_name(op_handler)), result);

    if (hdr->id)
        gb_operation_send_response(operation, result);
    op_mark_send_time(operation);
}

static bool gb_operation_has_timedout(struct gb_operation *operation)
{
    struct timespec current_time;
    struct timespec timeout_time;

    timeout_time.tv_sec = operation->time.tv_sec +
                          TIMEOUT_IN_MS / ONE_SEC_IN_MSEC;
    timeout_time.tv_nsec = operation->time.tv_nsec +
                          (TIMEOUT_IN_MS % ONE_SEC_IN_MSEC) * ONE_MSEC_IN_NSEC;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    if (current_time.tv_sec > timeout_time.tv_sec)
        return true;

    if (current_time.tv_sec < timeout_time.tv_sec)
        return false;

    return current_time.tv_nsec > timeout_time.tv_nsec;
}

/**
 * Update watchdog state
 *
 * Cancel cport watchdog if there is no outgoing message waiting for a response,
 * or update the watchdog if there is still outgoing messages.
 *
 * TODO use fine-grain timeout delay when doing the update
 *
 * @note This function should be called from an atomic context
 */
static void gb_watchdog_update(unsigned int cport)
{
    int flags;

    flags = irq_lock();

    if (list_is_empty(&g_cport[cport].tx_fifo)) {
        wd_cancel(&g_cport[cport].timeout_wd);
    } else {
        wd_start(&g_cport[cport].timeout_wd, TIMEOUT_WD_DELAY,
                 gb_operation_timeout, 1, cport);
    }

    irq_unlock(flags);
}

static void gb_clean_timedout_operation(unsigned int cport)
{
    int flags;
    struct list_head *iter, *iter_next;
    struct gb_operation *op;

    list_foreach_safe(&g_cport[cport].tx_fifo, iter, iter_next) {
        op = list_entry(iter, struct gb_operation, list);

        if (!gb_operation_has_timedout(op)) {
            continue;
        }

        flags = irq_lock();
        list_del(iter);
        irq_unlock(flags);

        if (op->callback) {
            op->callback(op);
        }
        gb_operation_unref(op);
    }

    gb_watchdog_update(cport);
}

static void gb_process_response(struct gb_operation_hdr *hdr,
                                struct gb_operation *operation)
{
    int flags;
    struct list_head *iter, *iter_next;
    struct gb_operation *op;
    struct gb_operation_hdr *op_hdr;

    list_foreach_safe(&g_cport[operation->cport].tx_fifo, iter, iter_next) {
        op = list_entry(iter, struct gb_operation, list);
        op_hdr = op->request_buffer;

        if (hdr->id != op_hdr->id)
            continue;

        flags = irq_lock();
        list_del(iter);
        gb_watchdog_update(operation->cport);
        irq_unlock(flags);

        /* attach this response with the original request */
        gb_operation_ref(operation);
        op->response = operation;
        op_mark_recv_time(op);
        if (op->callback)
            op->callback(op);
        gb_operation_unref(op);
        return;
    }

    LOG_ERR("CPort %u: cannot find matching request for response %hu. Dropping message.",
             operation->cport, sys_le16_to_cpu(hdr->id));
}

static void *gb_pending_message_worker(void *data)
{
    const int cportid = (intptr_t) data;
    int flags;
    struct gb_operation *operation;
    struct list_head *head;
    struct gb_operation_hdr *hdr;
    int retval;

    while (1) {
        retval = sem_wait(&g_cport[cportid].rx_fifo_lock);
        if (retval < 0)
            continue;

        if (g_cport[cportid].exit_worker &&
            list_is_empty(&g_cport[cportid].rx_fifo)) {
            break;
        }

        flags = irq_lock();
        head = g_cport[cportid].rx_fifo.next;
        list_del(g_cport[cportid].rx_fifo.next);
        irq_unlock(flags);

        operation = list_entry(head, struct gb_operation, list);
        hdr = operation->request_buffer;

        if (hdr == &timedout_hdr) {
            gb_clean_timedout_operation(cportid);
            continue;
        }

        if (hdr->type & GB_TYPE_RESPONSE_FLAG)
            gb_process_response(hdr, operation);
        else
            gb_process_request(hdr, operation);
        gb_operation_destroy(operation);
    }

    return NULL;
}

#if defined(CONFIG_UNIPRO_ZERO_COPY)
static struct gb_operation *gb_rx_create_operation(unsigned cport, void *data,
                                                   size_t size)
{
    struct gb_operation *op;

    op = _gb_operation_create(cport);
    if (!op)
        return NULL;

    op->is_unipro_rx_buf = true;
    op->request_buffer = data;

    return op;
}
#else
static struct gb_operation *gb_rx_create_operation(unsigned cport, void *data,
                                                   size_t size)
{
    struct gb_operation *op;

    op = gb_operation_create(cport, 0, size - sizeof(struct gb_operation_hdr));
    if (!op)
        return NULL;

    memcpy(op->request_buffer, data, size);

    return op;
}
#endif

int greybus_rx_handler(unsigned int cport, void *data, size_t size)
{
    int flags;
    struct gb_operation *op;
    struct gb_operation_hdr *hdr = data;
    struct gb_operation_handler *op_handler;
    size_t hdr_size;

    gb_loopback_log_entry(cport);
    if (cport >= cport_count || !data) {
        LOG_ERR("Invalid cport number: %u", cport);
        return -EINVAL;
    }

    if (!g_cport[cport].driver || !g_cport[cport].driver->op_handlers) {
        LOG_ERR("Cport %u does not have a valid driver registered", cport);
        return 0;
    }

    if (sizeof(*hdr) > size) {
        LOG_ERR("Dropping garbage request");
        return -EINVAL; /* Dropping garbage request */
    }

    hdr_size = sys_le16_to_cpu(hdr->size);

    if (hdr_size > size || sizeof(*hdr) > hdr_size) {
        LOG_ERR("Dropping garbage request");
        return -EINVAL; /* Dropping garbage request */
    }

    //LOG_HEXDUMP_DBG(data, size, "RX: ");

    if (gb_tape && gb_tape_fd >= 0) {
        struct gb_tape_record_header record_hdr = {
            .size = size,
            .cport = cport,
        };

        gb_tape->write(gb_tape_fd, &record_hdr, sizeof(record_hdr));
        gb_tape->write(gb_tape_fd, data, size);
    }

    op_handler = find_operation_handler(hdr->type, cport);
    if (op_handler && op_handler->fast_handler) {
        LOG_DBG("%s", gb_handler_name(op_handler));
        op_handler->fast_handler(cport, data);
        return 0;
    }

    op = gb_rx_create_operation(cport, data, hdr_size);
    if (!op)
        return -ENOMEM;

    op_mark_recv_time(op);

    flags = irq_lock();
    list_add(&g_cport[cport].rx_fifo, &op->list);
    sem_post(&g_cport[cport].rx_fifo_lock);
    irq_unlock(flags);

    return 0;
}

static void gb_flush_tx_fifo(unsigned int cport)
{
    struct list_head *iter, *iter_next;

    list_foreach_safe(&g_cport[cport].tx_fifo, iter, iter_next) {
        struct gb_operation *op = list_entry(iter, struct gb_operation, list);

        list_del(iter);
        gb_operation_unref(op);
    }
}

int gb_unregister_driver(unsigned int cport)
{
    if (cport >= cport_count || !g_cport[cport].driver || !transport_backend)
        return -EINVAL;

    if (transport_backend->stop_listening)
        transport_backend->stop_listening(cport);

    wd_cancel(&g_cport[cport].timeout_wd);

    g_cport[cport].exit_worker = true;
    sem_post(&g_cport[cport].rx_fifo_lock);
    pthread_join(g_cport[cport].thread, NULL);

    gb_flush_tx_fifo(cport);

    if (g_cport[cport].driver->exit)
        g_cport[cport].driver->exit(cport, g_cport[cport].driver->bundle);
    g_cport[cport].driver = NULL;

    return 0;
}

int _gb_register_driver(unsigned int cport, int bundle_id,
                        struct gb_driver *driver)
{
    pthread_attr_t thread_attr;
    pthread_attr_t *thread_attr_ptr = &thread_attr;
    struct gb_bundle *bundle;
	char thread_name[CONFIG_THREAD_MAX_NAME_LEN];
    int retval;

    LOG_DBG("Registering Greybus driver on CP%u", cport);

    if (cport >= cport_count) {
        LOG_ERR("Invalid cport number %u", cport);
        return -EINVAL;
    }

    if (!driver) {
        LOG_ERR("No driver to register");
        return -EINVAL;
    }

    if (g_cport[cport].driver) {
        LOG_ERR("%s is already registered for CP%u",
                 gb_driver_name(g_cport[cport].driver), cport);
        return -EEXIST;
    }

    if (!driver->op_handlers && driver->op_handlers_count > 0) {
        LOG_ERR("Invalid driver");
        return -EINVAL;
    }

    if (bundle_id >= 0 && bundle_id > manifest_get_max_bundle_id()) {
        LOG_ERR("invalid bundle_id: %d", bundle_id);
        return -EINVAL;
    }

    if (bundle_id >= 0 && !g_bundle[bundle_id]) {
        /*
         * TODO We should probably add a mechanism to destroy the bundle
         * objects allocated here, but since for now we don't really use any
         * actual shutdown procedure we'll leave it as a TODO.
         *
         * Eventually we'd need some reference counting mechanism, because we
         * call _gb_register_driver() once per used cport. We would need to
         * know when when there are no more cports referencing given bundle
         * and it's safe to free it.
         */
        bundle = calloc(1, sizeof(struct gb_bundle));
        if (!bundle)
            return -ENOMEM;

        bundle->id = bundle_id;
        g_bundle[bundle_id] = bundle;
    } else {
        bundle = NULL;
    }

    driver->bundle = bundle;

    if (driver->init) {
        retval = driver->init(cport, bundle);
        if (retval) {
            LOG_ERR("Can not init %s", gb_driver_name(driver));
            return retval;
        }
    }

    if (driver->op_handlers) {
        qsort(driver->op_handlers, driver->op_handlers_count,
              sizeof(*driver->op_handlers), gb_compare_handlers);
    }

    g_cport[cport].exit_worker = false;

    if (!driver->stack_size)
        driver->stack_size = DEFAULT_STACK_SIZE;

    retval = pthread_attr_init(&thread_attr);
    if (retval) {
        LOG_ERR("pthread_attr_init() failed (%d)", retval);
        goto pthread_attr_init_error;
    }

    retval = pthread_attr_setstacksize(&thread_attr, driver->stack_size);
    if (retval) {
        LOG_ERR("pthread_attr_setstacksize() failed (%d)", retval);
        goto pthread_attr_setstacksize_error;
    }

    retval = pthread_create(&g_cport[cport].thread, &thread_attr,
                            gb_pending_message_worker, (void *)((intptr_t) cport));
    if (retval) {
        LOG_ERR("pthread_create() failed (%d)", retval);
        goto pthread_create_error;
    }

	snprintf(thread_name, sizeof(thread_name), "greybus[%u]", cport);
	pthread_setname_np(g_cport[cport].thread, thread_name);

    pthread_attr_destroy(&thread_attr);
    thread_attr_ptr = NULL;

    g_cport[cport].driver = driver;

    return 0;

pthread_create_error:
pthread_attr_setstacksize_error:
    if (thread_attr_ptr != NULL)
        pthread_attr_destroy(&thread_attr);
pthread_attr_init_error:
    LOG_ERR("Can not create thread for %s", gb_driver_name(driver));
    if (driver->exit)
        driver->exit(cport, bundle);
    return retval;
}

int gb_listen(unsigned int cport)
{
    DEBUGASSERT(transport_backend);
    DEBUGASSERT(transport_backend->listen);

    if (cport >= cport_count) {
        LOG_ERR("Invalid cport number %u", cport);
        return -EINVAL;
    }

    if (!g_cport[cport].driver) {
        LOG_ERR("No driver registered! Can not connect CP%u.", cport);
        return -EINVAL;
    }

    return transport_backend->listen(cport);
}

int gb_stop_listening(unsigned int cport)
{
    DEBUGASSERT(transport_backend);
    DEBUGASSERT(transport_backend->stop_listening);

    if (cport >= cport_count) {
        LOG_ERR("Invalid cport number %u", cport);
        return -EINVAL;
    }

    if (!g_cport[cport].driver) {
        LOG_ERR("No driver registered! Can not disconnect CP%u.",
                 cport);
        return -EINVAL;
    }

    return transport_backend->stop_listening(cport);
}

static void gb_operation_timeout(int argc, uint32_t cport, ...)
{
    int flags;

    flags = irq_lock();

    /* timedout operation could potentially already been queued */
    if (!list_is_empty(&g_cport[cport].timedout_operation.list)) {
        irq_unlock(flags);
        return;
    }

    list_add(&g_cport[cport].rx_fifo, &g_cport[cport].timedout_operation.list);
    sem_post(&g_cport[cport].rx_fifo_lock);
    irq_unlock(flags);
}

static int gb_operation_send_request_nowait_cb(int status, const void *buf,
                                               void *priv)
{
    struct gb_operation *operation = priv;
    struct gb_operation_hdr *hdr = operation->request_buffer;

    hdr->result = status ? GB_OP_INTERNAL : 0;

    if (operation->callback) {
        operation->callback(operation);
    }

    gb_operation_unref(operation);

    return 0;
}

int gb_operation_send_request_nowait(struct gb_operation *operation,
                                     gb_operation_callback callback,
                                     bool need_response)
{
    struct gb_operation_hdr *hdr = operation->request_buffer;
    int retval = 0;
    int flags;

    DEBUGASSERT(operation);
    DEBUGASSERT(transport_backend);
    DEBUGASSERT(transport_backend->send_async);

    if (g_cport[operation->cport].exit_worker) {
        return -ENETDOWN;
    }

    if (need_response) {
        return -ENOTSUP;
    }

    hdr->id = 0;
    operation->callback = callback;

    //LOG_HEXDUMP_DBG(operation->request_buffer, hdr->size, "TX: ");

    gb_operation_ref(operation);

    flags = irq_lock();
    retval = transport_backend->send_async(operation->cport,
                                           operation->request_buffer,
                                           sys_le16_to_cpu(hdr->size),
                                           gb_operation_send_request_nowait_cb,
                                           operation);
    op_mark_send_time(operation);
    irq_unlock(flags);

    return retval;
}

int gb_operation_send_request(struct gb_operation *operation,
                              gb_operation_callback callback,
                              bool need_response)
{
    struct gb_operation_hdr *hdr = operation->request_buffer;
    int retval = 0;
    int flags;

    DEBUGASSERT(operation);
    DEBUGASSERT(transport_backend);
    DEBUGASSERT(transport_backend->send);

    if (g_cport[operation->cport].exit_worker)
        return -ENETDOWN;

    hdr->id = 0;

    flags = irq_lock();

    if (need_response) {
        hdr->id = sys_cpu_to_le16(atomic_inc(&request_id));
        if (hdr->id == 0) /* ID 0 is for request with no response */
            hdr->id = sys_cpu_to_le16(atomic_inc(&request_id));
        clock_gettime(CLOCK_MONOTONIC, &operation->time);
        operation->callback = callback;
        gb_operation_ref(operation);
        list_add(&g_cport[operation->cport].tx_fifo, &operation->list);
        if (!WDOG_ISACTIVE(&g_cport[operation->cport].timeout_wd)) {
            wd_start(&g_cport[operation->cport].timeout_wd, TIMEOUT_WD_DELAY,
                     gb_operation_timeout, 1, operation->cport);
        }
    }

    //LOG_HEXDUMP_DBG(operation->request_buffer, hdr->size, "TX: ");
    retval = transport_backend->send(operation->cport,
                                     operation->request_buffer,
                                     sys_le16_to_cpu(hdr->size));
    op_mark_send_time(operation);
    if (need_response && retval) {
        list_del(&operation->list);
        gb_watchdog_update(operation->cport);
        gb_operation_unref(operation);
    }

    irq_unlock(flags);

    return retval;
}

static void gb_operation_callback_sync(struct gb_operation *operation)
{
    sem_post(&operation->sync_sem);
}

int gb_operation_send_request_sync(struct gb_operation *operation)
{
    int retval;

    sem_init(&operation->sync_sem, 0, 0);

    retval =
        gb_operation_send_request(operation, gb_operation_callback_sync, true);
    if (retval)
        return retval;

    do {
        retval = sem_wait(&operation->sync_sem);
    } while (retval < 0 && errno == EINTR);

    return retval;
}

static int gb_operation_send_oom_response(struct gb_operation *operation)
{
    int retval;
    int flags;
    struct gb_operation_hdr *req_hdr = operation->request_buffer;

    if (g_cport[operation->cport].exit_worker)
        return -ENETDOWN;

    flags = irq_lock();

    oom_hdr.id = req_hdr->id;
    oom_hdr.type = GB_TYPE_RESPONSE_FLAG | req_hdr->type;

    retval = transport_backend->send(operation->cport, &oom_hdr,
                                     sizeof(oom_hdr));

    irq_unlock(flags);

    return retval;
}

int gb_operation_send_response(struct gb_operation *operation, uint8_t result)
{
    struct gb_operation_hdr *resp_hdr;
    int retval;
    bool has_allocated_response = false;

    DEBUGASSERT(operation);
    DEBUGASSERT(transport_backend);
    DEBUGASSERT(transport_backend->send);

    if (g_cport[operation->cport].exit_worker)
        return -ENETDOWN;

    if (operation->has_responded)
        return -EINVAL;

    if (!operation->response_buffer) {
        gb_operation_alloc_response(operation, 0);
        if (!operation->response_buffer)
            return gb_operation_send_oom_response(operation);

        has_allocated_response = true;
    }

    resp_hdr = operation->response_buffer;
    resp_hdr->result = result;

    //LOG_HEXDUMP_DBG(operation->response_buffer, resp_hdr->size, "TX: ");
    gb_loopback_log_exit(operation->cport, operation, resp_hdr->size);
    retval = transport_backend->send(operation->cport,
                                     operation->response_buffer,
                                     sys_le16_to_cpu(resp_hdr->size));
    if (retval) {
        LOG_ERR("Greybus backend failed to send: error %d", retval);
        if (has_allocated_response) {
            LOG_DBG("Free the response buffer");
            transport_backend->free_buf(operation->response_buffer);
            operation->response_buffer = NULL;
        }
        return retval;
    }

    operation->has_responded = true;
    return retval;
}

void *gb_operation_alloc_response(struct gb_operation *operation, size_t size)
{
    struct gb_operation_hdr *req_hdr;
    struct gb_operation_hdr *resp_hdr;

    DEBUGASSERT(operation);

    operation->response_buffer =
        transport_backend->alloc_buf(size + sizeof(*resp_hdr));
    if (!operation->response_buffer) {
        LOG_ERR("Can not allocate a response_buffer");
        return NULL;
    }

    memset(operation->response_buffer, 0, size + sizeof(*resp_hdr));

    req_hdr = operation->request_buffer;
    resp_hdr = operation->response_buffer;

    resp_hdr->size = sys_cpu_to_le16(size + sizeof(*resp_hdr));
    resp_hdr->id = req_hdr->id;
    resp_hdr->type = GB_TYPE_RESPONSE_FLAG | req_hdr->type;
    return gb_operation_get_response_payload(operation);
}

void gb_operation_destroy(struct gb_operation *operation)
{
    DEBUGASSERT(operation);
    gb_operation_unref(operation);
}

void gb_operation_ref(struct gb_operation *operation)
{
    DEBUGASSERT(operation);
    DEBUGASSERT(atomic_get(&operation->ref_count) > 0);
    atomic_inc(&operation->ref_count);
}

void gb_operation_unref(struct gb_operation *operation)
{
    DEBUGASSERT(operation);
    DEBUGASSERT(atomic_get(&operation->ref_count) > 0);

    /* zephyr atomic_dec(), via z_impl_atomic_sub(), returns the value
     * of the variable before it was decremented (much like the behaviour
     * defined in stdatomic.h with atomic_fetch_sub() */
    uint32_t ref_count = atomic_dec(&operation->ref_count) - 1;
    if (ref_count != 0) {
        return;
    }

    if (operation->is_unipro_rx_buf) {
        unipro_rxbuf_free(operation->cport, operation->request_buffer);
    } else {
        transport_backend->free_buf(operation->request_buffer);
    }

    transport_backend->free_buf(operation->response_buffer);
    if (operation->response) {
        gb_operation_unref(operation->response);
    }
    free(operation);
}

static struct gb_operation *_gb_operation_create(unsigned int cport)
{
    struct gb_operation *operation;

    if (cport >= cport_count)
        return NULL;

    operation = malloc(sizeof(*operation));
    if (!operation)
        return NULL;

    memset(operation, 0, sizeof(*operation));
    operation->cport = cport;

    list_init(&operation->list);
    atomic_init(&operation->ref_count, 1);

    return operation;
}

struct gb_operation *gb_operation_create(unsigned int cport, uint8_t type,
                                         uint32_t req_size)
{
    struct gb_operation *operation;
    struct gb_operation_hdr *hdr;

    if (cport >= cport_count)
        return NULL;

    operation = _gb_operation_create(cport);
    if (!operation) {
        return NULL;
    }

    operation->request_buffer =
        transport_backend->alloc_buf(req_size + sizeof(*hdr));
    if (!operation->request_buffer)
        goto malloc_error;

    memset(operation->request_buffer, 0, req_size + sizeof(*hdr));
    hdr = operation->request_buffer;
    hdr->size = sys_cpu_to_le16(req_size + sizeof(*hdr));
    hdr->type = type;

    return operation;
malloc_error:
    free(operation);
    return NULL;
}

size_t gb_operation_get_request_payload_size(struct gb_operation *operation)
{
    struct gb_operation_hdr *hdr;

    if (!operation || !operation->request_buffer) {
        return 0;
    }

    hdr = operation->request_buffer;
    if (sys_le16_to_cpu(hdr->size) < sizeof(*hdr)) {
        return 0;
    }

    return sys_le16_to_cpu(hdr->size) - sizeof(*hdr);
}

uint8_t gb_operation_get_request_result(struct gb_operation *operation)
{
    struct gb_operation_hdr *hdr;

    if (!operation) {
        return GB_OP_INTERNAL;
    }

    if (!operation->response) {
        return GB_OP_TIMEOUT;
    }

    hdr = operation->response->request_buffer;
    if (!hdr || hdr->size < sizeof(*hdr)) {
        return GB_OP_INTERNAL;
    }

    return hdr->result;
}

struct gb_bundle *gb_operation_get_bundle(struct gb_operation *operation)
{
    if (!operation) {
        return NULL;
    }

    return operation->bundle;
}

int gb_init(struct gb_transport_backend *transport)
{
    size_t num_bundles = manifest_get_max_bundle_id() + 1;
    int i;

    if (!transport)
        return -EINVAL;

    g_bundle = calloc(1, sizeof(struct gb_bundle *) * num_bundles);
    if (!g_bundle) {
        return -ENOMEM;
    }

    cport_count = unipro_cport_count();
    g_cport = calloc(1, sizeof(struct gb_cport_driver) * cport_count);
    if (!g_cport) {
        free(g_bundle);
        return -ENOMEM;
    }

    for (i = 0; i < cport_count; i++) {
        sem_init(&g_cport[i].rx_fifo_lock, 0, 0);
        list_init(&g_cport[i].rx_fifo);
        list_init(&g_cport[i].tx_fifo);
        wd_static(&g_cport[i].timeout_wd);
        g_cport[i].timedout_operation.request_buffer = &timedout_hdr;
        list_init(&g_cport[i].timedout_operation.list);
    }

    atomic_init(&request_id, (uint32_t) 0);

    transport_backend = transport;
    transport_backend->init();

    return 0;
}

void gb_deinit(void)
{
    int i;

    if (!transport_backend)
        return; /* gb not initialized */

    for (i = 0; i < cport_count; i++) {
        gb_unregister_driver(i);

        wd_delete(&g_cport[i].timeout_wd);
        sem_destroy(&g_cport[i].rx_fifo_lock);
    }

    free(g_cport);

    if (transport_backend->exit)
        transport_backend->exit();
    transport_backend = NULL;
}

int gb_tape_register_mechanism(struct gb_tape_mechanism *mechanism)
{
    if (!mechanism || !mechanism->open || !mechanism->close ||
        !mechanism->read || !mechanism->write)
        return -EINVAL;

    if (gb_tape)
        return -EBUSY;

    gb_tape = mechanism;

    return 0;
}

int gb_tape_communication(const char *pathname)
{
    if (!gb_tape)
        return -EINVAL;

    if (gb_tape_fd >= 0)
        return -EBUSY;

    gb_tape_fd = gb_tape->open(pathname, GB_TAPE_WRONLY);
    if (gb_tape_fd < 0)
        return gb_tape_fd;

    return 0;
}

int gb_tape_stop(void)
{
    if (!gb_tape || gb_tape_fd < 0)
        return -EINVAL;

    gb_tape->close(gb_tape_fd);
    gb_tape_fd = -EBADF;

    return 0;
}

int gb_tape_replay(const char *pathname)
{
    struct gb_tape_record_header hdr;
    char *buffer;
    ssize_t nread;
    int retval = 0;
    int fd;

    if (!pathname || !gb_tape)
        return -EINVAL;

    LOG_DBG("greybus: replaying '%s'...", pathname);

    fd = gb_tape->open(pathname, GB_TAPE_RDONLY);
    if (fd < 0)
        return fd;

    buffer = malloc(CPORT_BUF_SIZE);
    if (!buffer) {
        retval = -ENOMEM;
        goto error_buffer_alloc;
    }

    while (1) {
        nread = gb_tape->read(fd, &hdr, sizeof(hdr));
        if (!nread)
            break;

        if (nread != sizeof(hdr)) {
            LOG_ERR("gb-tape: invalid byte count read, aborting...");
            retval = -EIO;
            break;
        }

        nread = gb_tape->read(fd, buffer, hdr.size);
        if (hdr.size != nread) {
            LOG_ERR("gb-tape: invalid byte count read, aborting...");
            retval = -EIO;
            break;
        }

        greybus_rx_handler(hdr.cport, buffer, nread);
    }

    free(buffer);

error_buffer_alloc:
    gb_tape->close(fd);

    return retval;
}

int gb_notify(unsigned cport, enum gb_event event)
{
    if (cport >= cport_count)
        return -EINVAL;

    if (!g_cport[cport].driver)
        return -ENOTCONN;

    switch (event) {
    case GB_EVT_CONNECTED:
        if (g_cport[cport].driver->connected)
            g_cport[cport].driver->connected(cport);
        break;

    case GB_EVT_DISCONNECTED:
        if (g_cport[cport].driver->disconnected)
            g_cport[cport].driver->disconnected(cport);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

struct gb_bundle *gb_bundle_get_by_id(unsigned int bundle_id)
{
    if (bundle_id > manifest_get_max_bundle_id()) {
        return NULL;
    }

    return g_bundle[bundle_id];
}
