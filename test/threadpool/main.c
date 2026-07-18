/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <stdint.h>
#include <stdio.h>
#include <sgl-core.h>
#include <sgl-osal.h>

#define SGL_TEST_MEMORY_POOL_SIZE    (1024U * 1024U)
#define SGL_TEST_SUBMITTER_COUNT     (4U)
#define SGL_TEST_ITERATION_COUNT     (2000U)
#define SGL_TEST_TASK_COUNT          (8U)
#define SGL_TEST_WORKER_COUNT        (8U)

typedef struct {
    uint32_t execution_count;
} sgl_test_threadpool_task_t;

typedef struct {
    sgl_threadpool_t *threadpool;
    int result;
} sgl_test_submitter_context_t;

static SGL_ALIGNED(64) unsigned char
    sgl_test_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

static void sgl_test_threadpool_routine(void *current, void *cookie)
{
    sgl_test_threadpool_task_t *task;

    SGL_UNUSED_PARAM(cookie);
    /* cppcheck-suppress misra-c2012-11.5 */
    task = (sgl_test_threadpool_task_t *)current;
    task->execution_count++;
}

/*
 * Each submitter owns its task array, while all submitters share one pool.
 * attach_routine_consuming() is synchronous, so a task must be incremented
 * exactly once before its owner publishes the next iteration.
 *
 *   submitter[0..3] -> serialized publication -> shared worker generation
 *                   <- synchronous completion <- operation queue drained
 */
static sgl_osal_thread_return_t sgl_test_threadpool_submitter(
    sgl_osal_thread_arg_t argument)
{
    sgl_test_submitter_context_t *context;
    sgl_test_threadpool_task_t tasks[SGL_TEST_TASK_COUNT];
    sgl_queue_t *queue;
    sgl_size_t iteration;
    sgl_size_t task_index;

    /* cppcheck-suppress misra-c2012-11.5 */
    context = (sgl_test_submitter_context_t *)argument;
    for (task_index = 0U; task_index < SGL_TEST_TASK_COUNT; ++task_index) {
        tasks[task_index].execution_count = 0U;
    }

    for (iteration = 0U;
         (context->result == 0) &&
         (iteration < SGL_TEST_ITERATION_COUNT);
         ++iteration) {
        queue = sgl_queue_create(SGL_TEST_TASK_COUNT);
        if (queue == SGL_NULL) {
            context->result = 1;
        }
        for (task_index = 0U;
             (context->result == 0) &&
             (task_index < SGL_TEST_TASK_COUNT);
            ++task_index) {
            if (sgl_queue_unsafe_enqueue(queue, &tasks[task_index]) !=
                SGL_QUEUE_IS_NOT_FULL) {
                context->result = 1;
            }
        }
        if ((context->result == 0) &&
            (sgl_threadpool_attach_routine_consuming(
                 context->threadpool,
                 sgl_test_threadpool_routine,
                 queue,
                 SGL_NULL) != SGL_SUCCESS)) {
            context->result = 1;
        }
        sgl_queue_destroy(&queue);
    }

    for (task_index = 0U;
         (context->result == 0) && (task_index < SGL_TEST_TASK_COUNT);
         ++task_index) {
        if (tasks[task_index].execution_count != SGL_TEST_ITERATION_COUNT) {
            context->result = 1;
        }
    }

    EXIT_ROUTINE
}

static int sgl_test_threadpool_preserves_queue(sgl_threadpool_t *threadpool)
{
    sgl_test_threadpool_task_t tasks[SGL_TEST_TASK_COUNT];
    sgl_queue_t *queue;
    sgl_size_t task_index;
    int result;

    result = 0;
    queue = sgl_queue_create(SGL_TEST_TASK_COUNT);
    if (queue == SGL_NULL) {
        result = 1;
    }
    for (task_index = 0U;
         (result == 0) && (task_index < SGL_TEST_TASK_COUNT);
         ++task_index) {
        tasks[task_index].execution_count = 0U;
        if (sgl_queue_unsafe_enqueue(queue, &tasks[task_index]) !=
            SGL_QUEUE_IS_NOT_FULL) {
            result = 1;
        }
    }
    if ((result == 0) &&
        (sgl_threadpool_attach_routine(
             threadpool,
             sgl_test_threadpool_routine,
             queue,
             SGL_NULL) != SGL_SUCCESS)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_queue_get_count(queue) != SGL_TEST_TASK_COUNT)) {
        result = 1;
    }
    for (task_index = 0U;
         (result == 0) && (task_index < SGL_TEST_TASK_COUNT);
         ++task_index) {
        if (tasks[task_index].execution_count != 1U) {
            result = 1;
        }
    }
    sgl_queue_destroy(&queue);

    return result;
}

int main(void)
{
    sgl_test_submitter_context_t contexts[SGL_TEST_SUBMITTER_COUNT];
    sgl_osal_thread_t submitters[SGL_TEST_SUBMITTER_COUNT];
    sgl_threadpool_t *threadpool;
    sgl_size_t created_submitters;
    sgl_size_t index;
    int result;

    result = 0;
    created_submitters = 0U;
    threadpool = SGL_NULL;
    if (sgl_memory_pool_initialize(
            sgl_test_memory_pool,
            sizeof(sgl_test_memory_pool)) != SGL_SUCCESS) {
        result = 1;
    }
    if (result == 0) {
        threadpool = sgl_threadpool_create(
            SGL_TEST_WORKER_COUNT,
            SGL_TEST_SUBMITTER_COUNT,
            "threadpool-test");
        if (threadpool == SGL_NULL) {
            result = 1;
        }
    }
    if ((result == 0) &&
        (sgl_test_threadpool_preserves_queue(threadpool) != 0)) {
        result = 1;
    }

    for (index = 0U;
         (result == 0) && (index < SGL_TEST_SUBMITTER_COUNT);
         ++index) {
        contexts[index].threadpool = threadpool;
        contexts[index].result = 0;
        submitters[index] = sgl_thread_create(
            sgl_test_threadpool_submitter,
            &contexts[index]);
        if (submitters[index] == NULL_THREAD) {
            result = 1;
        }
        else {
            created_submitters++;
        }
    }
    for (index = 0U; index < created_submitters; ++index) {
        sgl_osal_thread_join(submitters[index]);
        if (contexts[index].result != 0) {
            result = 1;
        }
    }

    if (threadpool != SGL_NULL) {
        if (sgl_threadpool_destroy(threadpool) != SGL_SUCCESS) {
            result = 1;
        }
    }
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }
    if (result == 0) {
        (void)printf(
            "threadpool stress: %u operations passed\n",
            (unsigned int)(SGL_TEST_TASK_COUNT +
                (SGL_TEST_SUBMITTER_COUNT * SGL_TEST_ITERATION_COUNT *
                 SGL_TEST_TASK_COUNT)));
    }

    return result;
}
