#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sgl.h"
#include "sgl-osal.h"

typedef struct {
    sgl_queue_t *in, *out;
    sgl_osal_mutex_t lock;
    sgl_osal_cond_t cond;
    sgl_threadpool_routine_t routine;
    void *cookie;
    volatile bool is_done;
    volatile bool is_removable;
} sgl_threadpool_routine_handle_t;

struct sgl_threadpool {
    const char *base_name;
    size_t num_threads;
    sgl_osal_thread_t *threads;
    sgl_osal_mutex_t lock;
    sgl_osal_cond_t cond;
    sgl_queue_t *routine_lists;
    volatile bool is_idle_threadpool;
    volatile bool is_exit_threadpool;
};

static sgl_osal_thread_return_t sgl_threadpool_routine(sgl_osal_thread_arg_t arg);

static inline void sgl_threadpool_routine_handle_initialize(sgl_threadpool_routine_handle_t *routine_handle, sgl_queue_t *in, sgl_threadpool_routine_t routine, void *cookie)
{
    size_t capacity;

    capacity = sgl_queue_get_capacity(in);
    routine_handle->in = in;
    routine_handle->out = sgl_queue_create(capacity);
    routine_handle->routine = routine;
    routine_handle->cookie = cookie;
    routine_handle->is_done = false;
    routine_handle->is_removable = false;
    sgl_osal_mutex_init(&routine_handle->lock);
    sgl_osal_cond_init(&routine_handle->cond);
}

static inline void sgl_threadpool_routine_handle_deinitialize(sgl_threadpool_routine_handle_t *routine_handle)
{
    size_t i, count;
    const void *current;

    sgl_queue_copy(routine_handle->in, routine_handle->out);
    sgl_queue_destroy(&routine_handle->out);

    sgl_osal_mutex_destroy(&routine_handle->lock);
    sgl_osal_cond_destroy(&routine_handle->cond);

    memset(routine_handle, 0, sizeof(sgl_threadpool_routine_handle_t));
}

sgl_threadpool_t *sgl_threadpool_create(size_t num_threads, size_t max_routine_lists, const char *base_name)
{
    sgl_threadpool_t *pool = NULL;
    size_t i;

    /* create instance handle */
    pool = (sgl_threadpool_t *)malloc(sizeof(sgl_threadpool_t));
    if (pool == NULL) {
        return NULL;
    }
    pool->num_threads = num_threads;
    pool->is_exit_threadpool = false;
    
    /* create queue for task list */
    pool->routine_lists = sgl_queue_create(max_routine_lists);
    if (pool->routine_lists == NULL) {
        goto release_resource;
    }

    /* create mutex & conditional variable */
    sgl_osal_mutex_init(&pool->lock);
    sgl_osal_cond_init(&pool->cond);

    /* allocate thread basket */
    pool->threads = (sgl_osal_thread_t *)malloc(num_threads * sizeof(sgl_osal_thread_t));
    if (pool->threads == NULL) {
        goto release_resource;
    }

    /* create threads */
    pool->base_name = base_name;
    for (i = 0; i < num_threads; ++i) {
        pool->threads[i] = sgl_thread_create(sgl_threadpool_routine, (sgl_osal_thread_arg_t)pool);
    }

    goto exit_function;

release_resource:
    SGL_SAFE_FREE(pool->threads);
    SGL_SAFE_FREE(pool);

exit_function:
    return pool;
}

sgl_result_t sgl_threadpool_destroy(sgl_threadpool_t *pool)
{
    sgl_result_t result = SGL_SUCCESS;

    if (pool != NULL) {
        /* Set exit flag so worker threads can break out of their loop */
        pool->is_exit_threadpool = true;
    
        /* Wake up all worker threads that might be waiting on the condition variable */
        sgl_osal_mutex_lock(&pool->lock);
        sgl_osal_cond_broadcast(&pool->cond);
        sgl_osal_mutex_unlock(&pool->lock);
    
       /* Join all worker threads to ensure they have finished execution */
        for (size_t i = 0; i < pool->num_threads; ++i) {
            if (pool->threads[i] != NULL_THREAD) {
                sgl_osal_thread_join(pool->threads[i]);
            }
        }
    
        /* Destroy the routine list queue */
        if (pool->routine_lists != NULL) {
            sgl_queue_destroy(&pool->routine_lists);
        }
    
        /* Destroy synchronization primitives */
        sgl_osal_mutex_destroy(&pool->lock);
        sgl_osal_cond_destroy(&pool->cond);
    
        /* Free thread array */
        SGL_SAFE_FREE(pool->threads);
    
        /* Free the pool object itself */
        free(pool);
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}



sgl_result_t sgl_threadpool_attach_routine(sgl_threadpool_t *pool, sgl_threadpool_routine_t routine, sgl_queue_t *operations, void *cookie)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_threadpool_routine_handle_t routine_handle;
  
    if ((pool != NULL) && (routine != NULL) && (operations != NULL)) {
        /* initialize routine handle */
        sgl_threadpool_routine_handle_initialize(&routine_handle, operations, routine, cookie);
    
        /* attach routine */
        sgl_queue_enqueue(pool->routine_lists, (const void *)&routine_handle);
    
        /* broadcast */
        sgl_osal_cond_broadcast(&pool->cond);
    
        /* wait for routine... */
        sgl_osal_mutex_lock(&routine_handle.lock);
        while (routine_handle.is_done == false) {
            sgl_osal_cond_wait(&routine_handle.cond, &routine_handle.lock);
        }
        sgl_osal_mutex_unlock(&routine_handle.lock);

        /* wait for routine to be removable */
        while (routine_handle.is_removable == false) {
            sgl_osal_yield_thread();
        }
    
        /* deinitialize routine handle */
        sgl_threadpool_routine_handle_deinitialize(&routine_handle);
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wcast-qual"
#   pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#elif defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wcast-qual"
#   pragma clang diagnostic ignored "-Wdiscarded-qualifiers"
#endif
static sgl_osal_thread_return_t sgl_threadpool_routine(sgl_osal_thread_arg_t arg)
{
    sgl_threadpool_t *pool = (sgl_threadpool_t *)arg;
    sgl_threadpool_routine_handle_t *routine_handle;
    void *current;
    sgl_result_t result;

    while (pool->is_exit_threadpool == false) {
        /* wait for a routine to be attached */
        routine_handle = (sgl_threadpool_routine_handle_t *)sgl_queue_peek(pool->routine_lists);
        if (routine_handle != NULL) {
            /* get routine handle */
            current = sgl_queue_dequeue(routine_handle->in);
            if (current != NULL) {
                /* execute routine */
                routine_handle->routine(current, routine_handle->cookie);

                /* record completed routine */
                sgl_queue_enqueue(routine_handle->out, current);
                result = sgl_queue_is_full(routine_handle->out);
                if (result == SGL_QUEUE_IS_FULL) {
                    /* signal */
                    sgl_osal_mutex_lock(&routine_handle->lock);
                    routine_handle->is_done = true;
                    sgl_osal_cond_signal(&routine_handle->cond);
                    sgl_osal_mutex_unlock(&routine_handle->lock);

                    /* mark routine as removable */
                    routine_handle->is_removable = true;
                }
            }
            else {
                /* remove routine */
                (void)sgl_queue_dequeue(pool->routine_lists);
            }
        }
        else {
            /* wait for a routine to be attached */
            sgl_osal_mutex_lock(&pool->lock);
            sgl_osal_cond_wait(&pool->cond, &pool->lock);
            sgl_osal_mutex_unlock(&pool->lock);
        }
    }

    EXIT_ROUTINE
}
#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#elif defined(__clang__)
#   pragma clang diagnostic pop
#endif
