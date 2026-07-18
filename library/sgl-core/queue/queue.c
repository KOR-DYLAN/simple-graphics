/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "sgl-osal.h"
#include "sgl_trace.h"
#include <sgl_memory_cast.h>

struct sgl_queue {
    void **data;
    sgl_size_t capacity;
    sgl_size_t count;
    sgl_size_t head;
    sgl_size_t tail;
    sgl_osal_spinlock_t lock;
};

#define SGL_QUEUE_CACHE_LINE_SIZE    (64U)
#define SGL_QUEUE_DATA_OFFSET \
    ((sizeof(sgl_queue_t) + SGL_QUEUE_CACHE_LINE_SIZE - 1U) & \
     ~(SGL_QUEUE_CACHE_LINE_SIZE - 1U))

#if defined(SGL_CFG_HAS_LTTNG)
/*
 * A profiling build pays one trylock only to distinguish uncontended queue
 * access from the shared-ring contention interval shown in Trace Compass.
 */
static SGL_ALWAYS_INLINE void sgl_queue_profiled_lock(
    sgl_queue_t *queue,
    const char *operation)
{
    if (sgl_osal_spinlock_try_lock(&queue->lock) == SGL_FALSE) {
        SGL_TRACE_QUEUE_LOCK_CONTENDED(queue, operation);
        sgl_osal_spinlock_lock(&queue->lock);
        SGL_TRACE_QUEUE_LOCK_ACQUIRED(queue, operation);
    }
}
#else
#define sgl_queue_profiled_lock(queue, operation) \
    sgl_osal_spinlock_lock(&(queue)->lock)
#endif

static SGL_ALWAYS_INLINE void **sgl_queue_data_address(sgl_queue_t *queue)
{
    sgl_uint8_t *address;

    /* cppcheck-suppress misra-c2012-11.3 */
    address = (sgl_uint8_t *)queue;

    return sgl_memory_as_void_ptr_array(&address[SGL_QUEUE_DATA_OFFSET]);
}

static SGL_ALWAYS_INLINE void *sgl_queue_as_void_ptr(const void *data)
{
    sgl_uintptr_t data_addr;
    void *result;

    /*
     * SGL-QUEUE-DEV-001:
     * The queue transports opaque object pointers without dereferencing or
     * taking ownership.  The public enqueue API accepts const void * so callers
     * can pass immutable objects, while the existing dequeue API returns void *.
     * Keep that legacy API shape and isolate the pointer/integer/pointer bridge
     * here instead of suppressing Rule 11.6 for the whole translation unit.
     */
    /* cppcheck-suppress misra-c2012-11.6 */
    data_addr = (sgl_uintptr_t)data;
    /* cppcheck-suppress misra-c2012-11.6 */
    result = (void *)data_addr;

    return result;
}

sgl_queue_t *sgl_queue_create(sgl_size_t capacity)
{
    sgl_queue_t *queue = SGL_NULL;
    sgl_size_t allocation_size;
    sgl_size_t maximum_capacity;

    /*
     * Queue allocation layout
     * -----------------------
     * Keep metadata and the pointer ring in one allocation to remove one
     * allocator lock/unlock pair from each short-lived resize submission.
     * A cache-line-sized gap prevents the spinlock/count writes from sharing
     * a line with the first operation pointers consumed by worker threads.
     *
     *   allocation
     *   +---------------- metadata ----------------+ gap + pointer ring ...
     *   ^ queue                                    ^ queue->data
     */
    maximum_capacity =
        (SGL_SIZE_MAX - SGL_QUEUE_DATA_OFFSET) / sizeof(void *);
    if ((0U < capacity) && (capacity <= maximum_capacity)) {
        allocation_size = SGL_QUEUE_DATA_OFFSET +
            (sizeof(void *) * capacity);
        queue = sgl_memory_as_queue(sgl_malloc(allocation_size));
        if (queue != SGL_NULL) {
            queue->data = sgl_queue_data_address(queue);
            queue->head = 0U;
            queue->tail = 0U;
            queue->count = 0U;
            queue->capacity = capacity;
            sgl_osal_spinlock_init(&queue->lock);
        }
    }

    return queue;
}

void sgl_queue_destroy(sgl_queue_t **queue)
{
    if (queue != SGL_NULL) {
        if (*queue != SGL_NULL) {
            sgl_osal_spinlock_destroy(&(*queue)->lock);
            sgl_free(*queue);
            *queue = SGL_NULL;
        }
    }
}

sgl_result_t sgl_queue_copy(sgl_queue_t *SGL_RESTRICT dst, const sgl_queue_t *SGL_RESTRICT src)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((dst != SGL_NULL) && (src != SGL_NULL)) {
        if (src->capacity <= dst->capacity) {
            dst->count = src->count;
            dst->head = src->head;
            dst->tail = src->tail;
            (void)sgl_memcpy(dst->data, src->data, sizeof(const void *) * src->capacity);
        }
        else {
            result = SGL_ERROR_MISSMATCHED_CAPACITY;
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

sgl_result_t sgl_queue_unsafe_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_size_t head;

    if ((queue != SGL_NULL) && (data != SGL_NULL)) {
        result = sgl_queue_is_full(queue);
        if (SGL_LIKELY(result == SGL_QUEUE_IS_NOT_FULL)) {
            head = queue->head;
            queue->data[head] = sgl_queue_as_void_ptr(data);
            if (queue->capacity <= ++head) {
                head = 0;
            }
            queue->head = head;
            queue->count++;
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

sgl_result_t sgl_queue_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_size_t head;

    if ((queue != SGL_NULL) && (data != SGL_NULL)) {
        sgl_queue_profiled_lock(queue, SGL_TRACE_QUEUE_ENQUEUE);
        result = sgl_queue_is_full(queue);
        if (SGL_LIKELY(result == SGL_QUEUE_IS_NOT_FULL)) {
            head = queue->head;
            queue->data[head] = sgl_queue_as_void_ptr(data);
            if (queue->capacity <= ++head) {
                head = 0;
            }
            queue->head = head;
            queue->count++;
        }
        sgl_osal_spinlock_unlock(&queue->lock);
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

void *sgl_queue_dequeue(sgl_queue_t *queue)
{
    sgl_result_t result;
    void *data = SGL_NULL;
    sgl_size_t tail;

    if (queue != SGL_NULL) {
        sgl_queue_profiled_lock(queue, SGL_TRACE_QUEUE_DEQUEUE);
        result = sgl_queue_is_empty(queue);
        if (SGL_LIKELY(result == SGL_QUEUE_IS_NOT_EMPTY)) {
            tail = queue->tail;
            data = queue->data[tail];
            if (queue->capacity <= ++tail) {
                tail = 0;
            }
            queue->tail = tail;
            queue->count--;
        }
        sgl_osal_spinlock_unlock(&queue->lock);
    }

    return data;
}

void *sgl_queue_peek(sgl_queue_t *queue)
{
    sgl_result_t result;
    void *data = SGL_NULL;
    sgl_size_t tail;

    if (queue != SGL_NULL) {
        sgl_queue_profiled_lock(queue, SGL_TRACE_QUEUE_PEEK);
        result = sgl_queue_is_empty(queue);
        if (SGL_LIKELY(result == SGL_QUEUE_IS_NOT_EMPTY)) {
            tail = queue->tail;
            data = queue->data[tail];
        }
        sgl_osal_spinlock_unlock(&queue->lock);
    }

    return data;
}

sgl_result_t sgl_queue_is_empty(const sgl_queue_t *queue)
{
    return (queue->count == 0U) ? SGL_QUEUE_IS_EMPTY : SGL_QUEUE_IS_NOT_EMPTY;
}

sgl_result_t sgl_queue_is_full(const sgl_queue_t *queue)
{
    return (queue->count == queue->capacity) ? SGL_QUEUE_IS_FULL : SGL_QUEUE_IS_NOT_FULL;
}

sgl_size_t sgl_queue_get_capacity(const sgl_queue_t *queue)
{
    return (queue != SGL_NULL) ? queue->capacity : (sgl_size_t)0U;
}

sgl_size_t sgl_queue_get_count(const sgl_queue_t *queue)
{
    return (queue != SGL_NULL) ? queue->count : (sgl_size_t)0U;
}
