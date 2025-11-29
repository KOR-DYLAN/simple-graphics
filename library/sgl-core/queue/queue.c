#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"
#include "sgl-osal.h"

struct sgl_queue {
    const void **data;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    sgl_osal_mutex_t lock;
};

static sgl_result_t sgl_queue_is_empty(sgl_queue_t *queue);
static sgl_result_t sgl_queue_is_full(sgl_queue_t *queue);

sgl_queue_t *sgl_queue_create(size_t capacity)
{
    sgl_queue_t *queue = NULL;

    if (0 < capacity) {
        queue = (sgl_queue_t *)malloc(sizeof(sgl_queue_t));
        if (queue != NULL) {
            queue->data = (const void **)malloc(sizeof(const void *) * capacity);
            if (queue->data != NULL) {
                queue->head = 0;
                queue->tail = 0;
                queue->count = 0;
                queue->capacity = capacity;
                sgl_osal_mutex_init(&queue->lock);
            }
            else {
                free(queue);
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
            sgl_osal_mutex_destroy(&(*queue)->lock);
            free((*queue)->data);
            free(*queue);
            *queue = NULL;
        }
    }
}

sgl_result_t sgl_queue_enqueue(sgl_queue_t *queue, const void *data)
{
    sgl_result_t result = SGL_SUCCESS;
    size_t head;

    if ((queue != NULL) && (data != NULL)) {
        sgl_osal_mutex_lock(&queue->lock);
        result = sgl_queue_is_full(queue);
        if (result == SGL_QUEUE_IS_NOT_FULL) {
            head = queue->head;
            queue->data[head] = data;
            if (queue->capacity <= ++head) {
                head = 0;
            }
            queue->head = head;
            queue->count++;
        }
        sgl_osal_mutex_unlock(&queue->lock);
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

const void *sgl_queue_dequeue(sgl_queue_t *queue)
{
    sgl_result_t result;
    const void *data = NULL;
    size_t tail;

    if (queue != NULL) {
        sgl_osal_mutex_lock(&queue->lock);
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
        sgl_osal_mutex_unlock(&queue->lock);
    }

    return data;
}

const void *sgl_queue_peek(sgl_queue_t *queue)
{
    sgl_result_t result;
    const void *data = NULL;
    size_t tail;

    if (queue != NULL) {
        sgl_osal_mutex_lock(&queue->lock);
        result = sgl_queue_is_empty(queue);
        if (result == SGL_QUEUE_IS_NOT_EMPTY) {
            tail = queue->tail;
            data = queue->data[tail];
        }
        sgl_osal_mutex_unlock(&queue->lock);
    }

    return data;
}

static sgl_result_t sgl_queue_is_empty(sgl_queue_t *queue)
{
    return (queue->count == 0) ? SGL_QUEUE_IS_EMPTY : SGL_QUEUE_IS_NOT_EMPTY;
}

static sgl_result_t sgl_queue_is_full(sgl_queue_t *queue)
{
    return (queue->count == queue->capacity) ? SGL_QUEUE_IS_FULL : SGL_QUEUE_IS_NOT_FULL;
}
