/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
/*
 * SGL-MEM-DEV-001:
 * SGL keeps the allocator interface compatible with malloc/calloc by returning
 * void *.  These inline conversion helpers isolate the MISRA C:2012 Rule 11.5
 * deviations at the allocation/context boundary instead of repeating
 * suppressions at each call site.
 */
#ifndef SGL_MEMORY_CAST_H_
#define SGL_MEMORY_CAST_H_

#include <sgl-core.h>
#include <sgl-fixed_point.h>

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_memory_as_uint8(void *memory)
{
    sgl_uint8_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_uint8_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE const sgl_uint8_t *sgl_memory_as_const_uint8(const void *memory)
{
    const sgl_uint8_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (const sgl_uint8_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE unsigned char *sgl_memory_as_uchar(void *memory)
{
    unsigned char *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (unsigned char *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_int32_t *sgl_memory_as_int32(void *memory)
{
    sgl_int32_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_int32_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_t *sgl_memory_as_q11(void *memory)
{
    sgl_q11_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_q11_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t *sgl_memory_as_q11_ext(void *memory)
{
    sgl_q11_ext_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_q11_ext_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE void **sgl_memory_as_void_ptr_array(void *memory)
{
    void **result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (void **)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_queue_t *sgl_memory_as_queue(void *memory)
{
    sgl_queue_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_queue_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_threadpool_t *sgl_memory_as_threadpool(void *memory)
{
    sgl_threadpool_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_threadpool_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_nearest_neighbor_lookup_t *sgl_memory_as_nearest_neighbor_lookup(void *memory)
{
    sgl_nearest_neighbor_lookup_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_nearest_neighbor_lookup_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_bilinear_lookup_t *sgl_memory_as_bilinear_lookup(void *memory)
{
    sgl_bilinear_lookup_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_bilinear_lookup_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_bicubic_lookup_t *sgl_memory_as_bicubic_lookup(void *memory)
{
    sgl_bicubic_lookup_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_bicubic_lookup_t *)memory;

    return result;
}

#endif  /* SGL_MEMORY_CAST_H_ */
