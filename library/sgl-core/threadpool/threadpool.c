/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
/*
 * SGL-CALLBACK-DEV-001: the generic queue and thread entry points transport
 * typed routine state through the required void-pointer callback interface.
 */
#include <sgl-core.h>
#include "sgl-osal.h"
#include "sgl_trace.h"
#include <sgl_memory_cast.h>

#if !defined(SGL_THREADPOOL_SPIN_WAIT_ITERATIONS)
#define SGL_THREADPOOL_SPIN_WAIT_ITERATIONS    (65536U)
#endif

struct sgl_threadpool {
    const char *base_name;
    sgl_size_t num_threads;
    sgl_osal_thread_t *threads;
    sgl_osal_mutex_t lock;
    sgl_osal_cond_t worker_cond;
    sgl_osal_cond_t submitter_cond;
    sgl_threadpool_routine_t routine;
    void *SGL_RESTRICT cookie;
    sgl_queue_t *operations;
    sgl_queue_t *completed_operations;
    sgl_size_t active_workers;
    sgl_size_t claimed_workers;
    sgl_size_t worker_limit;
    sgl_osal_atomic_uint32_t routine_generation;
    sgl_osal_atomic_uint32_t completion_generation;
    sgl_bool_t has_routine;
    sgl_bool_t is_done;
    sgl_bool_t is_exit_threadpool;
};

/*
 * Threadpool routine ownership
 * ----------------------------
 * attach_routine() publishes one active routine at a time into pool-owned
 * storage.  Because this API is synchronous, the submitting thread executes
 * operations while up to num_threads - 1 workers join the same generation.
 * This work-first path avoids paying a worker wake-up before useful work can
 * begin and keeps the requested number of compute threads unchanged.
 *
 * Worker publication and submit completion use separate condition variables.
 * A completion broadcast therefore wakes waiting submitters without waking
 * every idle worker and sending it immediately back to sleep.
 *
 *   submitting thread                       worker threads
 *   -----------------                       --------------
 *   reserve operation
 *   publish generation  ------------------> reserve + claim generation
 *   execute + dequeue  <---- shared queue -> execute + dequeue
 *   finish caller       ------------------+ finish worker
 *                                          |
 *   wait/clear routine  <--- submitter_cond +--- last participant
 *
 * pool->lock protects publication, active worker count, done flag, and exit
 * flag.  Release/acquire generation counters bound active spinning before the
 * condition-variable fallback.  Queue spinlocks protect only queue contents.
 * The worker loop exits only through the locked claim path, so ThreadSanitizer
 * sees a single synchronization contract without serializing execution.
 */
typedef struct {
    sgl_threadpool_routine_t routine;
    void *cookie;
    sgl_queue_t *operations;
    sgl_queue_t *completed_operations;
    void *first_operation;
} sgl_threadpool_routine_context_t;

static SGL_ALWAYS_INLINE sgl_osal_thread_t *sgl_threadpool_memory_as_osal_thread(void *memory)
{
    sgl_osal_thread_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_osal_thread_t *)memory;

    return result;
}

static sgl_osal_thread_return_t sgl_threadpool_routine(sgl_osal_thread_arg_t arg);
static void sgl_threadpool_clear_routine(sgl_threadpool_t *pool);
static sgl_queue_t *sgl_threadpool_create_completed_queue(const sgl_queue_t *operations);
static sgl_bool_t sgl_threadpool_try_claim_routine(
    sgl_threadpool_t *pool,
    sgl_threadpool_routine_context_t *routine,
    sgl_uint32_t *last_generation);
static void sgl_threadpool_spin_until_changed(
    const sgl_osal_atomic_uint32_t *generation,
    sgl_uint32_t expected);
static void sgl_threadpool_execute_routine(
    const sgl_threadpool_routine_context_t *routine,
    const sgl_threadpool_t *pool,
    sgl_uint32_t generation,
    const char *role);
static void sgl_threadpool_finish_routine(sgl_threadpool_t *pool);
static sgl_result_t sgl_threadpool_attach_routine_internal(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_threadpool_routine_t routine,
    sgl_queue_t *SGL_RESTRICT operations,
    void *SGL_RESTRICT cookie,
    sgl_bool_t preserve_operations);

static void sgl_threadpool_clear_routine(sgl_threadpool_t *pool)
{
    pool->routine = SGL_NULL;
    pool->cookie = SGL_NULL;
    pool->operations = SGL_NULL;
    pool->completed_operations = SGL_NULL;
    pool->active_workers = 0U;
    pool->claimed_workers = 0U;
    pool->worker_limit = 0U;
    pool->has_routine = SGL_FALSE;
    pool->is_done = SGL_FALSE;
}

static sgl_queue_t *sgl_threadpool_create_completed_queue(const sgl_queue_t *operations)
{
    sgl_queue_t *completed_operations;
    sgl_size_t operation_count;

    operation_count = sgl_queue_get_count(operations);
    if (operation_count > 0U) {
        completed_operations = sgl_queue_create(operation_count);
    }
    else {
        completed_operations = SGL_NULL;
    }

    return completed_operations;
}

static void sgl_threadpool_spin_until_changed(
    const sgl_osal_atomic_uint32_t *generation,
    sgl_uint32_t expected)
{
    sgl_uint32_t iteration;

    for (iteration = 0U;
         (iteration < SGL_THREADPOOL_SPIN_WAIT_ITERATIONS) &&
         (sgl_osal_atomic_uint32_load_acquire(generation) == expected);
         ++iteration) {
        SGL_CPU_RELAX();
    }
}

static sgl_bool_t sgl_threadpool_try_claim_routine(
    sgl_threadpool_t *pool,
    sgl_threadpool_routine_context_t *routine,
    sgl_uint32_t *last_generation)
{
    sgl_bool_t is_claimed;
    sgl_uint32_t generation;

    is_claimed = SGL_FALSE;
    routine->routine = SGL_NULL;
    routine->cookie = SGL_NULL;
    routine->operations = SGL_NULL;
    routine->completed_operations = SGL_NULL;
    routine->first_operation = SGL_NULL;

    sgl_threadpool_spin_until_changed(
        &pool->routine_generation, *last_generation);
    sgl_osal_mutex_lock(&pool->lock);
    while ((pool->is_exit_threadpool == SGL_FALSE) &&
           (is_claimed == SGL_FALSE)) {
        while ((pool->is_exit_threadpool == SGL_FALSE) &&
               ((pool->has_routine == SGL_FALSE) ||
                (pool->is_done == SGL_TRUE) ||
                (pool->claimed_workers >= pool->worker_limit) ||
                (sgl_osal_atomic_uint32_load_acquire(
                     &pool->routine_generation) == *last_generation))) {
            sgl_osal_cond_wait(&pool->worker_cond, &pool->lock);
        }

        if (pool->is_exit_threadpool == SGL_FALSE) {
            generation = sgl_osal_atomic_uint32_load_acquire(
                &pool->routine_generation);
            routine->first_operation = sgl_queue_dequeue(pool->operations);
            if (routine->first_operation != SGL_NULL) {
                routine->routine = pool->routine;
                routine->cookie = pool->cookie;
                routine->operations = pool->operations;
                routine->completed_operations = pool->completed_operations;
                *last_generation = generation;
                pool->active_workers++;
                pool->claimed_workers++;
                is_claimed = SGL_TRUE;
            }
            else {
                /*
                 * Another participant reserved the final operation before this
                 * worker acquired pool->lock.  Mark this generation observed
                 * and sleep until new work is published without inflating the
                 * active-worker count for an empty participant.
                 */
                *last_generation = generation;
            }
        }
    }
    sgl_osal_mutex_unlock(&pool->lock);

    return is_claimed;
}

static void sgl_threadpool_execute_routine(
    const sgl_threadpool_routine_context_t *routine,
    const sgl_threadpool_t *pool,
    sgl_uint32_t generation,
    const char *role)
{
    void *current;
#if defined(SGL_CFG_HAS_LTTNG)
    sgl_size_t completed_operations;

    completed_operations = 0U;
    SGL_TRACE_THREADPOOL_PARTICIPANT_BEGIN(pool, generation, role);
#else
    SGL_UNUSED(pool);
    SGL_UNUSED(generation);
    SGL_UNUSED(role);
#endif
    current = routine->first_operation;
    while (current != SGL_NULL) {
        routine->routine(current, routine->cookie);
#if defined(SGL_CFG_HAS_LTTNG)
        completed_operations++;
#endif
        if (routine->completed_operations != SGL_NULL) {
            (void)sgl_queue_enqueue(routine->completed_operations, current);
        }
        current = sgl_queue_dequeue(routine->operations);
    }
#if defined(SGL_CFG_HAS_LTTNG)
    SGL_TRACE_THREADPOOL_PARTICIPANT_END(
        pool, generation, role, completed_operations);
#endif
}

static void sgl_threadpool_finish_routine(sgl_threadpool_t *pool)
{
    sgl_osal_mutex_lock(&pool->lock);
    if (pool->active_workers > 0U) {
        pool->active_workers--;
    }
    if (pool->active_workers == 0U) {
        pool->is_done = SGL_TRUE;
        (void)sgl_osal_atomic_uint32_increment_release(
            &pool->completion_generation);
        sgl_osal_cond_broadcast(&pool->submitter_cond);
    }
    sgl_osal_mutex_unlock(&pool->lock);
}

sgl_threadpool_t *sgl_threadpool_create(sgl_size_t num_threads, sgl_size_t max_routine_lists, const char *base_name)
{
    sgl_threadpool_t *pool = SGL_NULL;
    sgl_size_t i;

    /*
     * max_routine_lists is retained as part of the public creation contract.
     * The current implementation serializes attach_routine() calls internally,
     * so a positive value means "routine submission is supported".
     */
    if ((num_threads > 0U) && (max_routine_lists > 0U)) {
        /* create instance handle */
        pool = sgl_memory_as_threadpool(sgl_calloc(1, sizeof(sgl_threadpool_t)));
    }

    if (pool != SGL_NULL) {
        pool->num_threads = num_threads;
        pool->is_exit_threadpool = SGL_FALSE;
        pool->routine_generation = 0U;
        pool->completion_generation = 0U;

        /* create mutex & conditional variable */
        sgl_osal_mutex_init(&pool->lock);
        sgl_osal_cond_init(&pool->worker_cond);
        sgl_osal_cond_init(&pool->submitter_cond);

        /* allocate thread basket */
        pool->threads = sgl_threadpool_memory_as_osal_thread(
            sgl_malloc(num_threads * sizeof(sgl_osal_thread_t)));
        if (pool->threads != SGL_NULL) {
            /* create threads */
            pool->base_name = base_name;
            for (i = 0; i < num_threads; ++i) {
                pool->threads[i] = sgl_thread_create(sgl_threadpool_routine, (sgl_osal_thread_arg_t)pool);
            }
        }
        else {
            sgl_osal_mutex_destroy(&pool->lock);
            sgl_osal_cond_destroy(&pool->worker_cond);
            sgl_osal_cond_destroy(&pool->submitter_cond);
            sgl_free(pool);
            pool = SGL_NULL;
        }
    }

    return pool;
}

sgl_result_t sgl_threadpool_destroy(sgl_threadpool_t *pool)
{
    sgl_result_t result = SGL_SUCCESS;

    if (pool != SGL_NULL) {
        /*
         * Set exit flag and wake workers under the same lock they use to read
         * it.  Writing it outside the lock races with waiting workers.
         */
        sgl_osal_mutex_lock(&pool->lock);
        pool->is_exit_threadpool = SGL_TRUE;
        sgl_osal_cond_broadcast(&pool->worker_cond);
        sgl_osal_cond_broadcast(&pool->submitter_cond);
        sgl_osal_mutex_unlock(&pool->lock);

        /* Join all worker threads to ensure they have finished execution */
        for (sgl_size_t i = 0; i < pool->num_threads; ++i) {
            if (pool->threads[i] != NULL_THREAD) {
                sgl_osal_thread_join(pool->threads[i]);
            }
        }

        /* Destroy synchronization primitives */
        sgl_osal_mutex_destroy(&pool->lock);
        sgl_osal_cond_destroy(&pool->worker_cond);
        sgl_osal_cond_destroy(&pool->submitter_cond);

        /* Free thread array */
        SGL_SAFE_FREE(pool->threads);

        /* Free the pool object itself */
        sgl_free(pool);
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

sgl_size_t sgl_threadpool_get_num_threads(const sgl_threadpool_t *pool)
{
    sgl_size_t num_threads;

    num_threads = 0U;
    if (pool != SGL_NULL) {
        num_threads = pool->num_threads;
    }

    return num_threads;
}

static sgl_result_t sgl_threadpool_attach_routine_internal(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_threadpool_routine_t routine,
    sgl_queue_t *SGL_RESTRICT operations,
    void *SGL_RESTRICT cookie,
    sgl_bool_t preserve_operations)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_queue_t *completed_operations;
    sgl_threadpool_routine_context_t routine_context;
    sgl_uint32_t completion_generation;
    sgl_uint32_t routine_generation;
    sgl_size_t operation_count;

    completed_operations = SGL_NULL;
    routine_context.routine = routine;
    routine_context.cookie = cookie;
    routine_context.operations = operations;
    routine_context.completed_operations = SGL_NULL;
    routine_context.first_operation = SGL_NULL;
    if ((pool != SGL_NULL) && (routine != SGL_NULL) && (operations != SGL_NULL)) {
        operation_count = sgl_queue_get_count(operations);
        if ((preserve_operations == SGL_TRUE) && (operation_count > 0U)) {
            completed_operations = sgl_threadpool_create_completed_queue(operations);
            if (completed_operations == SGL_NULL) {
                result = SGL_ERROR_MEMORY_ALLOCATION;
            }
        }

        if (result == SGL_SUCCESS) {
            /*
             * The pool owns the active routine state while workers are running.
             * attach_routine() returns only after all operations have completed
             * and no worker still holds the caller's cookie pointer.
             */
            sgl_osal_mutex_lock(&pool->lock);
            while ((pool->has_routine == SGL_TRUE) &&
                   (pool->is_exit_threadpool == SGL_FALSE)) {
                sgl_osal_cond_wait(&pool->submitter_cond, &pool->lock);
            }

            if (pool->is_exit_threadpool == SGL_FALSE) {
                completion_generation =
                    sgl_osal_atomic_uint32_load_acquire(
                        &pool->completion_generation);
                pool->routine = routine;
                pool->cookie = cookie;
                pool->operations = operations;
                pool->completed_operations = completed_operations;
                routine_context.completed_operations = completed_operations;
                if (operation_count > 0U) {
                    routine_context.first_operation =
                        sgl_queue_dequeue(operations);
                }
                pool->active_workers = (routine_context.first_operation != SGL_NULL)
                    ? 1U
                    : 0U;
                pool->claimed_workers = 0U;
                pool->worker_limit = 0U;
                if ((operation_count > 1U) && (pool->num_threads > 1U)) {
                    pool->worker_limit = operation_count - 1U;
                    if (pool->worker_limit >= pool->num_threads) {
                        pool->worker_limit = pool->num_threads - 1U;
                    }
                }
                pool->has_routine = SGL_TRUE;
                pool->is_done = (pool->active_workers == 0U)
                    ? SGL_TRUE
                    : SGL_FALSE;
                routine_generation =
                    sgl_osal_atomic_uint32_increment_release(
                        &pool->routine_generation);
                SGL_TRACE_THREADPOOL_DISPATCH_BEGIN(
                    pool,
                    routine_generation,
                    operation_count,
                    pool->worker_limit,
                    pool->num_threads);
                if (pool->worker_limit == 1U) {
                    sgl_osal_cond_signal(&pool->worker_cond);
                }
                else if (pool->worker_limit > 1U) {
                    sgl_osal_cond_broadcast(&pool->worker_cond);
                }
                sgl_osal_mutex_unlock(&pool->lock);

                if (routine_context.first_operation != SGL_NULL) {
                    sgl_threadpool_execute_routine(
                        &routine_context,
                        pool,
                        routine_generation,
                        SGL_TRACE_ROLE_SUBMITTER);
                    sgl_threadpool_finish_routine(pool);
                    sgl_threadpool_spin_until_changed(
                        &pool->completion_generation,
                        completion_generation);
                }

                sgl_osal_mutex_lock(&pool->lock);
                while (pool->is_done == SGL_FALSE) {
                    SGL_TRACE_THREADPOOL_COMPLETION_WAIT_BEGIN(
                        pool, routine_generation);
                    sgl_osal_cond_wait(&pool->submitter_cond, &pool->lock);
                    SGL_TRACE_THREADPOOL_COMPLETION_WAIT_END(
                        pool, routine_generation);
                }

                if (completed_operations != SGL_NULL) {
                    (void)sgl_queue_copy(operations, completed_operations);
                }
                SGL_TRACE_THREADPOOL_DISPATCH_END(
                    pool, routine_generation, operation_count);
                sgl_threadpool_clear_routine(pool);
                sgl_osal_cond_broadcast(&pool->submitter_cond);
            }
            else {
                result = SGL_ERROR_INVALID_ARGUMENTS;
            }
            sgl_osal_mutex_unlock(&pool->lock);
        }

        sgl_queue_destroy(&completed_operations);
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

sgl_result_t sgl_threadpool_attach_routine(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_threadpool_routine_t routine,
    sgl_queue_t *SGL_RESTRICT operations,
    void *SGL_RESTRICT cookie)
{
    sgl_result_t result;

    result = sgl_threadpool_attach_routine_internal(
        pool, routine, operations, cookie, SGL_TRUE);

    return result;
}

sgl_result_t sgl_threadpool_attach_routine_consuming(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_threadpool_routine_t routine,
    sgl_queue_t *SGL_RESTRICT operations,
    void *SGL_RESTRICT cookie)
{
    sgl_result_t result;

    result = sgl_threadpool_attach_routine_internal(
        pool, routine, operations, cookie, SGL_FALSE);

    return result;
}

static sgl_osal_thread_return_t sgl_threadpool_routine(sgl_osal_thread_arg_t arg)
{
    sgl_threadpool_t *pool = sgl_memory_as_threadpool(arg);
    sgl_threadpool_routine_context_t routine;
    sgl_uint32_t last_generation;

    routine.routine = SGL_NULL;
    routine.cookie = SGL_NULL;
    routine.operations = SGL_NULL;
    routine.completed_operations = SGL_NULL;
    routine.first_operation = SGL_NULL;
    last_generation = 0U;
    while (sgl_threadpool_try_claim_routine(
               pool,
               &routine,
               &last_generation) == SGL_TRUE) {
        sgl_threadpool_execute_routine(
            &routine,
            pool,
            last_generation,
            SGL_TRACE_ROLE_WORKER);
        sgl_threadpool_finish_routine(pool);
    }

    EXIT_ROUTINE
}
