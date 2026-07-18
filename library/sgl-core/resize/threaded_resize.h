/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_THREADED_RESIZE_H_
#define SGL_THREADED_RESIZE_H_

#include <sgl-core.h>

#define SGL_RESIZE_CHUNKS_PER_WORKER          (4U)
#define SGL_RESIZE_LARGE_CHUNKS_PER_WORKER    (8U)
#define SGL_RESIZE_LARGE_HEIGHT               (480)
#define SGL_RESIZE_UNIFORM_CHUNKS_PER_WORKER  (1U)
#define SGL_RESIZE_NEAREST_MIN_THREAD_BYTES \
    ((sgl_uint64_t)2097152U)

#if defined(SGL_CFG_HAS_THREAD)
/*
 * Threaded resize row partitioning
 * --------------------------------
 * Fixed 4/8-row tasks make queue management dominate small and medium resize
 * calls.  Use a bounded number of tasks while preserving the minimum row unit
 * required by each scalar, SIMD, or cache-backed implementation.
 *
 *   destination rows
 *          |
 *          v
 *   workers x 4 target chunks (x 8 above 480 rows)
 *          |
 *          v
 *   round chunk rows up to minimum_bulk
 *          |
 *          +----> about four/eight queue operations per worker
 */
static SGL_ALWAYS_INLINE sgl_int32_t sgl_resize_thread_bulk_size(
    const sgl_threadpool_t *pool,
    sgl_int32_t d_height,
    sgl_int32_t minimum_bulk)
{
    sgl_size_t workers;
    sgl_size_t chunks_per_worker;
    sgl_size_t minimum;
    sgl_int32_t bulk_size;

    workers = sgl_threadpool_get_num_threads(pool);
    chunks_per_worker = SGL_RESIZE_CHUNKS_PER_WORKER;
    minimum = (sgl_size_t)minimum_bulk;
    bulk_size = minimum_bulk;

    if ((workers > 0U) && (d_height > 0) && (minimum > 0U)) {
        sgl_size_t target_operations;
        sgl_size_t rows_per_operation;

        if (d_height > SGL_RESIZE_LARGE_HEIGHT) {
            chunks_per_worker = SGL_RESIZE_LARGE_CHUNKS_PER_WORKER;
        }
        target_operations = workers * chunks_per_worker;
        rows_per_operation = (sgl_size_t)d_height / target_operations;
        if (((sgl_size_t)d_height % target_operations) != 0U) {
            rows_per_operation++;
        }

        if (rows_per_operation > minimum) {
            rows_per_operation =
                ((rows_per_operation + minimum - 1U) / minimum) * minimum;
            bulk_size = (sgl_int32_t)rows_per_operation;
        }
    }

    return bulk_size;
}

/* Uniform-cost row kernels do not need cache-oriented over-partitioning. */
static SGL_ALWAYS_INLINE sgl_int32_t sgl_resize_uniform_thread_bulk_size(
    const sgl_threadpool_t *pool,
    sgl_int32_t d_height,
    sgl_int32_t minimum_bulk)
{
    sgl_size_t minimum;
    sgl_size_t workers;
    sgl_int32_t bulk_size;

    minimum = (sgl_size_t)minimum_bulk;
    workers = sgl_threadpool_get_num_threads(pool);
    bulk_size = minimum_bulk;

    if ((workers > 0U) && (d_height > 0) && (minimum > 0U)) {
        sgl_size_t rows_per_operation;

        rows_per_operation = (sgl_size_t)d_height /
            (workers * SGL_RESIZE_UNIFORM_CHUNKS_PER_WORKER);
        if (((sgl_size_t)d_height %
             (workers * SGL_RESIZE_UNIFORM_CHUNKS_PER_WORKER)) != 0U) {
            rows_per_operation++;
        }

        if (rows_per_operation > minimum) {
            rows_per_operation =
                ((rows_per_operation + minimum - 1U) / minimum) * minimum;
            bulk_size = (sgl_int32_t)rows_per_operation;
        }
    }

    return bulk_size;
}

/*
 * Nearest-neighbor is memory-bound.  Below this output size, queue publication
 * and worker wake-up cost more than the row copies saved by parallelism.
 */
static SGL_ALWAYS_INLINE sgl_bool_t sgl_resize_nearest_should_use_threadpool(
    const sgl_threadpool_t *pool,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t bpp)
{
    sgl_uint64_t output_bytes;
    /* SGL_FALSE is the project's explicitly typed boolean sentinel. */
    /* cppcheck-suppress misra-c2012-10.5 */
    sgl_bool_t result = SGL_FALSE;

    output_bytes = (sgl_uint64_t)(sgl_uint32_t)d_width *
        (sgl_uint64_t)(sgl_uint32_t)d_height *
        (sgl_uint64_t)(sgl_uint32_t)bpp;
    if ((sgl_threadpool_get_num_threads(pool) > 1U) &&
        (output_bytes >= SGL_RESIZE_NEAREST_MIN_THREAD_BYTES)) {
        result = SGL_TRUE;
    }

    return result;
}
#endif  /* SGL_CFG_HAS_THREAD */

#endif  /* SGL_THREADED_RESIZE_H_ */
