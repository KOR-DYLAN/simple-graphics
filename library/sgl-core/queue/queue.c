#include <sgl-core.h>
#include "sgl-osal.h"
#include <sgl_memory_cast.h>

struct sgl_queue {
    void **data;
    sgl_size_t capacity;
    sgl_size_t count;
    sgl_size_t head;
    sgl_size_t tail;
    sgl_osal_spinlock_t lock;
};

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

    if (0U < capacity) {
        queue = sgl_memory_as_queue(sgl_malloc(sizeof(sgl_queue_t)));
        if (queue != SGL_NULL) {
            queue->data = sgl_memory_as_void_ptr_array(sgl_malloc(sizeof(void *) * capacity));
            if (queue->data != SGL_NULL) {
                queue->head = 0;
                queue->tail = 0;
                queue->count = 0;
                queue->capacity = capacity;
                sgl_osal_spinlock_init(&queue->lock);
            }
            else {
                sgl_free(queue);
                queue = SGL_NULL;
            }
        }
    }

    return queue;
}

void sgl_queue_destroy(sgl_queue_t **queue)
{
    if (queue != SGL_NULL) {
        if (*queue != SGL_NULL) {
            sgl_osal_spinlock_destroy(&(*queue)->lock);
            sgl_free((*queue)->data);
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
        if (result == SGL_QUEUE_IS_NOT_FULL) {
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
        sgl_osal_spinlock_lock(&queue->lock);
        result = sgl_queue_is_full(queue);
        if (result == SGL_QUEUE_IS_NOT_FULL) {
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
        sgl_osal_spinlock_lock(&queue->lock);
        result = sgl_queue_is_empty(queue);
        if (result == SGL_QUEUE_IS_NOT_EMPTY) {
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
        sgl_osal_spinlock_lock(&queue->lock);
        result = sgl_queue_is_empty(queue);
        if (result == SGL_QUEUE_IS_NOT_EMPTY) {
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
