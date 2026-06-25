/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
/* SGL-QUEUE-DEV-001: the generic queue transports opaque object pointers. */
/* cppcheck-suppress-file misra-c2012-11.6 */
#include <stdlib.h>
#include <string.h>
#include <sgl-core.h>
#include "sgl-osal.h"

struct sgl_queue {
    void **data;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    sgl_osal_spinlock_t lock;
};

sgl_queue_t *sgl_queue_create(size_t capacity)
{
    sgl_queue_t *queue = NULL;

    if (0U < capacity) {
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        queue = (sgl_queue_t *)sgl_malloc(sizeof(sgl_queue_t));
        if (queue != NULL) {
            /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
            /* cppcheck-suppress misra-c2012-11.5 */
            queue->data = (void **)sgl_malloc(sizeof(void *) * capacity);
            if (queue->data != NULL) {
                queue->head = 0;
                queue->tail = 0;
                queue->count = 0;
                queue->capacity = capacity;
                sgl_osal_spinlock_init(&queue->lock);
            }
            else {
                sgl_free(queue);
                queue = NULL;
            }
        }
    }

    return queue;
}

void sgl_queue_destroy(sgl_queue_t **queue)
{
    if (queue != NULL) {
        if (*queue != NULL) {
            sgl_osal_spinlock_destroy(&(*queue)->lock);
            sgl_free((*queue)->data);
            sgl_free(*queue);
            *queue = NULL;
        }
    }
}

sgl_result_t sgl_queue_copy(sgl_queue_t *SGL_RESTRICT dst, const sgl_queue_t *SGL_RESTRICT src)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((dst != NULL) && (src != NULL)) {
        if (src->capacity <= dst->capacity) {
            dst->count = src->count;
            dst->head = src->head;
            dst->tail = src->tail;
            (void)memcpy(dst->data, src->data, sizeof(const void *) * src->capacity);
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
    size_t head;
    const sgl_uintptr_t data_addr = (const sgl_uintptr_t)data;

    if ((queue != NULL) && (data != NULL)) {
        result = sgl_queue_is_full(queue);
        if (result == SGL_QUEUE_IS_NOT_FULL) {
            head = queue->head;
            queue->data[head] = (void *)data_addr;
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
    size_t head;
    const sgl_uintptr_t data_addr = (const sgl_uintptr_t)data;

    if ((queue != NULL) && (data != NULL)) {
        sgl_osal_spinlock_lock(&queue->lock);
        result = sgl_queue_is_full(queue);
        if (result == SGL_QUEUE_IS_NOT_FULL) {
            head = queue->head;
            queue->data[head] = (void *)data_addr;
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
    void *data = NULL;
    size_t tail;

    if (queue != NULL) {
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
    void *data = NULL;
    size_t tail;

    if (queue != NULL) {
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

size_t sgl_queue_get_capacity(const sgl_queue_t *queue)
{
    return (queue != NULL) ? queue->capacity : (size_t)0U;
}

size_t sgl_queue_get_count(const sgl_queue_t *queue)
{
    return (queue != NULL) ? queue->count : (size_t)0U;
}
