/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_PARALLEL_FOR_H_
#define SGL_PARALLEL_FOR_H_

#include <sgl-core.h>

typedef struct {
    sgl_int32_t start;
    sgl_int32_t count;
} sgl_parallel_range_t;

sgl_result_t sgl_parallel_for(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_threadpool_routine_t routine,
    void *SGL_RESTRICT cookie);

/* Use one contiguous range per worker for uniform, bandwidth-bound work. */
sgl_result_t sgl_parallel_for_static(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_threadpool_routine_t routine,
    void *SGL_RESTRICT cookie);

static SGL_ALWAYS_INLINE const sgl_parallel_range_t *
sgl_parallel_range_from_memory(const void *memory)
{
    const sgl_parallel_range_t *result;

    /* SGL-MEM-DEV-001: typed conversion from threadpool operation storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (const sgl_parallel_range_t *)memory;

    return result;
}

#endif  /* SGL_PARALLEL_FOR_H_ */
