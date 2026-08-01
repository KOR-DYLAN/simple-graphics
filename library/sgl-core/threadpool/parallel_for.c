/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include "parallel_for.h"

#include <sgl_memory_cast.h>

#define SGL_PARALLEL_DYNAMIC_CHUNKS_PER_WORKER  (4U)
#define SGL_PARALLEL_STATIC_CHUNKS_PER_WORKER   (1U)

#if defined(SGL_CFG_HAS_THREAD)
static sgl_parallel_range_t *sgl_parallel_ranges_from_memory(void *memory)
{
    sgl_parallel_range_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic allocation storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_parallel_range_t *)memory;

    return result;
}

static sgl_int32_t sgl_parallel_calculate_bulk(
    const sgl_threadpool_t *pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_size_t chunks_per_worker)
{
    sgl_size_t workers;
    sgl_size_t units;
    sgl_size_t target_operations;
    sgl_size_t units_per_operation;
    sgl_int32_t result;

    workers = sgl_threadpool_get_num_threads(pool);
    units = (sgl_size_t)(sgl_uint32_t)item_count /
        (sgl_size_t)(sgl_uint32_t)item_alignment;
    if (((sgl_size_t)(sgl_uint32_t)item_count %
         (sgl_size_t)(sgl_uint32_t)item_alignment) != 0U) {
        ++units;
    }
    if (workers >= ((units / chunks_per_worker) + 1U)) {
        target_operations = units;
    }
    else {
        target_operations = workers * chunks_per_worker;
    }
    units_per_operation = units / target_operations;
    if ((units % target_operations) != 0U) {
        ++units_per_operation;
    }
    units = units_per_operation *
        (sgl_size_t)(sgl_uint32_t)item_alignment;
    if (units > (sgl_size_t)(sgl_uint32_t)item_count) {
        units = (sgl_size_t)(sgl_uint32_t)item_count;
    }
    result = (sgl_int32_t)units;

    return result;
}

static sgl_int32_t sgl_parallel_calculate_operations(
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_int32_t bulk)
{
    sgl_int32_t result;
    sgl_int32_t remainder;

    result = item_count / bulk;
    remainder = item_count % bulk;
    if (remainder != 0) {
        ++result;
    }
    if ((result > 1) &&
        (remainder > 0) &&
        (remainder < item_alignment)) {
        --result;
    }

    return result;
}

static void sgl_parallel_initialize_ranges(
    sgl_parallel_range_t *ranges,
    sgl_queue_t *operations,
    sgl_int32_t operation_count,
    sgl_int32_t item_count,
    sgl_int32_t bulk)
{
    sgl_int32_t index;

    for (index = 0; index < operation_count; ++index) {
        ranges[index].start = index * bulk;
        ranges[index].count = bulk;
        (void)sgl_queue_unsafe_enqueue(
            operations, (const void *)&ranges[index]);
    }
    ranges[operation_count - 1].count =
        item_count - ranges[operation_count - 1].start;
}

static sgl_bool_t sgl_parallel_allocate_operations(
    sgl_int32_t operation_count,
    sgl_queue_t **operations,
    sgl_parallel_range_t **ranges)
{
    sgl_bool_t result = SGL_FALSE;

    *operations = sgl_queue_create(
        (sgl_size_t)(sgl_uint32_t)operation_count);
    *ranges = sgl_parallel_ranges_from_memory(sgl_malloc(
        sizeof(sgl_parallel_range_t) *
        (sgl_size_t)(sgl_uint32_t)operation_count));
    if ((*operations != SGL_NULL) && (*ranges != SGL_NULL)) {
        result = SGL_TRUE;
    }

    return result;
}

static void sgl_parallel_release_operations(
    sgl_queue_t **operations,
    sgl_parallel_range_t *ranges)
{
    sgl_queue_destroy(operations);
    SGL_SAFE_FREE(ranges);
}

static sgl_result_t sgl_parallel_dispatch(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_threadpool_routine_t routine,
    void *SGL_RESTRICT cookie,
    sgl_size_t chunks_per_worker)
{
    sgl_parallel_range_t *ranges;
    sgl_queue_t *operations;
    sgl_int32_t bulk;
    sgl_int32_t operation_count;
    sgl_result_t result = SGL_ERROR_MEMORY_ALLOCATION;

    ranges = SGL_NULL;
    operations = SGL_NULL;
    bulk = sgl_parallel_calculate_bulk(
        pool, item_count, item_alignment, chunks_per_worker);
    operation_count = sgl_parallel_calculate_operations(
        item_count, item_alignment, bulk);
    if (sgl_parallel_allocate_operations(
            operation_count, &operations, &ranges) == SGL_TRUE) {
        sgl_parallel_initialize_ranges(
            ranges, operations, operation_count, item_count, bulk);
        result = sgl_threadpool_attach_routine_consuming(
            pool, routine, operations, cookie);
    }

    sgl_parallel_release_operations(&operations, ranges);

    return result;
}
#endif  /* SGL_CFG_HAS_THREAD */

static sgl_result_t sgl_parallel_for_schedule(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_threadpool_routine_t routine,
    void *SGL_RESTRICT cookie,
    sgl_size_t chunks_per_worker)
{
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    if ((pool != SGL_NULL) &&
        (item_count > 0) &&
        (item_alignment > 0) &&
        (routine != SGL_NULL)) {
#if defined(SGL_CFG_HAS_THREAD)
        if (sgl_threadpool_get_num_threads(pool) > 0U) {
            result = sgl_parallel_dispatch(
                pool,
                item_count,
                item_alignment,
                routine,
                cookie,
                chunks_per_worker);
        }
#else
        (void)cookie;
        result = SGL_ERROR_NOT_SUPPORTED;
#endif  /* SGL_CFG_HAS_THREAD */
    }

    return result;
}

sgl_result_t sgl_parallel_for(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_threadpool_routine_t routine,
    void *SGL_RESTRICT cookie)
{
    sgl_result_t result;

    result = sgl_parallel_for_schedule(
        pool,
        item_count,
        item_alignment,
        routine,
        cookie,
        SGL_PARALLEL_DYNAMIC_CHUNKS_PER_WORKER);

    return result;
}

sgl_result_t sgl_parallel_for_static(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_threadpool_routine_t routine,
    void *SGL_RESTRICT cookie)
{
    sgl_result_t result;

    result = sgl_parallel_for_schedule(
        pool,
        item_count,
        item_alignment,
        routine,
        cookie,
        SGL_PARALLEL_STATIC_CHUNKS_PER_WORKER);

    return result;
}
