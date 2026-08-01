/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_CORE_H_
#define SGL_CORE_H_

#if defined(__cplusplus)
extern "C" {
#endif
/*
 *******************************************************************
 *                          INCLUDES
 *******************************************************************
 */
#include <sgl-compiler.h>
#include <sgl-type.h>
#include <sgl-fourcc.h>
#include <sgl-config.h>

/*
 *******************************************************************
 *                          DEFINES
 *******************************************************************
 */
#define SGL_UNUSED_PARAM(p)                         SGL_UNUSED(p)
#define SGL_DIV_ROUNDUP(n, d)                       (((n) + (d) - 1) / (d))
#define SGL_SAFE_FREE(p)                            if ((p) != SGL_NULL) { sgl_free((p)); (p) = SGL_NULL; }
#define SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS    (4U)
#define SGL_GENERIC_BULK_SIZE                       (4)
#define SGL_SIMD_BULK_SIZE                          (8)
#define SGL_BPP32                                   (4)
#define SGL_BPP24                                   (3)
#define SGL_BPP16                                   (2)
#define SGL_BPP8                                    (1)


/*
 *******************************************************************
 *                          TYPEDEF
 *******************************************************************
 */
typedef enum {
    SGL_SUCCESS,
    SGL_FAILURE,
    SGL_ERROR_INVALID_ARGUMENTS,
    SGL_ERROR_MEMORY_ALLOCATION,
    SGL_ERROR_MISSMATCHED_CAPACITY,
    SGL_ERROR_NOT_SUPPORTED,
    SGL_QUEUE_IS_EMPTY,
    SGL_QUEUE_IS_NOT_EMPTY,
    SGL_QUEUE_IS_FULL,
    SGL_QUEUE_IS_NOT_FULL,
} sgl_result_t;

typedef struct sgl_nearest_neighbor_lookup_table    sgl_nearest_neighbor_lookup_t;
typedef struct sgl_bilinear_lookup_table            sgl_bilinear_lookup_t;
typedef struct sgl_bicubic_lookup_table             sgl_bicubic_lookup_t;
typedef struct sgl_rotate_lookup_table              sgl_rotate_lookup_t;
typedef struct sgl_queue                            sgl_queue_t;
typedef struct sgl_threadpool                       sgl_threadpool_t;
typedef void(*sgl_threadpool_routine_t)(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);


/*******************************************************************
 *                          Memory Operations
 *******************************************************************/
/*
 * Copy operations require non-overlapping source and destination ranges.
 * Set operations store the low eight bits of value in every destination byte.
 *
 * sgl_memcpy() and sgl_memset() select the accelerated implementation when
 * the configured target provides one.
 */
void *sgl_memcpy(void *SGL_RESTRICT destination,
                 const void *SGL_RESTRICT source,
                 sgl_size_t size);
void *sgl_memset(void *destination, sgl_int32_t value, sgl_size_t size);


/*******************************************************************
 *                          Memory
 *******************************************************************/
/*
 * Registers a caller-owned buffer as the single process-wide SGL memory pool.
 * The buffer may be unaligned; the implementation adjusts the usable range.
 * The caller must keep the buffer alive and unchanged until deinitialization
 * succeeds. Initialize/deinitialize only while no other thread uses SGL memory.
 *
 * A pool must be initialized before any SGL operation that allocates memory.
 * sgl_malloc and sgl_calloc return NULL before initialization or when the pool
 * cannot satisfy the request. The implementation never falls back to the C
 * runtime heap.
 *
 * Deinitialization returns SGL_FAILURE while pool allocations remain alive.
 * sgl_free(NULL) is valid. Pointers outside the active pool are ignored. A pool
 * pointer must be released exactly once before successful deinitialization.
 *
 * MISRA deviation SGL-MEM-DEV-001:
 * MISRA C:2012 Rule 11.5 is deviated only in the public inline conversion
 * helpers declared by <sgl_memory_cast.h>. The allocator intentionally follows
 * the standard malloc interface and therefore returns void *. Allocation sites
 * restore the pointer type through those helpers, and the pool guarantees
 * alignment suitable for every supported object type.
 */
sgl_result_t sgl_memory_pool_initialize(void *memory, sgl_size_t size);
sgl_result_t sgl_memory_pool_deinitialize(void);
void *sgl_malloc(sgl_size_t size);
void *sgl_calloc(sgl_size_t count, sgl_size_t size);
void sgl_free(void *memory);


/*******************************************************************
 *                          Resize
 *******************************************************************/
sgl_nearest_neighbor_lookup_t *sgl_generic_create_nearest_neighbor_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height);
sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height);
sgl_bicubic_lookup_t *sgl_generic_create_bicubic_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height);

void sgl_generic_destroy_nearest_neighbor_lut(sgl_nearest_neighbor_lookup_t *lut);
void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut);
void sgl_generic_destroy_bicubic_lut(sgl_bicubic_lookup_t *lut);

/* Generic Resize */
sgl_result_t sgl_generic_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp);

sgl_result_t sgl_generic_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp);

sgl_result_t sgl_generic_resize_bicubic(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp);

/* SIMD Resize */
#if defined(SGL_CFG_HAS_SIMD)
sgl_result_t sgl_simd_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp);

sgl_result_t sgl_simd_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp);
sgl_result_t sgl_simd_resize_bicubic(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp);
#endif  /* !SGL_CFG_HAS_SIMD */

/*******************************************************************
 *                          Crop
 *******************************************************************/
/* A NULL pool selects the single-thread path. */
/* Generic Crop */
sgl_result_t sgl_generic_crop(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t src_width, sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t x, sgl_int32_t y, sgl_int32_t crop_width, sgl_int32_t crop_height,
    sgl_int32_t bpp);
/* SIMD Crop */
#if defined(SGL_CFG_HAS_SIMD)
sgl_result_t sgl_simd_crop(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t src_width, sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t x, sgl_int32_t y, sgl_int32_t crop_width, sgl_int32_t crop_height,
    sgl_int32_t bpp);
#endif  /* !SGL_CFG_HAS_SIMD */


/*******************************************************************
 *                          Rotate
 *******************************************************************/
sgl_result_t sgl_get_rotated_dimensions(
    sgl_int32_t src_width, sgl_int32_t src_height, sgl_float64_t angle,
    sgl_int32_t *dst_width, sgl_int32_t *dst_height);
sgl_int32_t sgl_get_rotated_size(sgl_int32_t src_width, sgl_int32_t src_height, sgl_int32_t bpp, sgl_float64_t angle);
/*
 * A rotate LUT is independent of bpp and may be reused only with the source
 * dimensions and angle used at creation. A NULL pool or LUT is valid.
 */
sgl_rotate_lookup_t *sgl_generic_create_rotate_lut(
    sgl_int32_t src_width, sgl_int32_t src_height, sgl_float64_t angle);
void sgl_generic_destroy_rotate_lut(sgl_rotate_lookup_t *lut);

/* Generic Rotate */
sgl_result_t sgl_generic_rotate(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_rotate_lookup_t *SGL_RESTRICT ext_lut,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t src_width, sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t bpp, sgl_float64_t angle);
/* SIMD Rotate */
#if defined(SGL_CFG_HAS_SIMD)
sgl_result_t sgl_simd_rotate(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_rotate_lookup_t *SGL_RESTRICT ext_lut,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t src_width, sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t bpp, sgl_float64_t angle);
#endif  /* !SGL_CFG_HAS_SIMD */


/*******************************************************************
 *                          Convert
 *******************************************************************/
/* A NULL pool selects the single-thread path. */
/* Generic Convert */
sgl_result_t sgl_generic_rgb2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
sgl_result_t sgl_generic_rgb2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
sgl_result_t sgl_generic_yuv2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
sgl_result_t sgl_generic_yuv2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
/* SIMD Convert */
#if defined(SGL_CFG_HAS_SIMD)
sgl_result_t sgl_simd_rgb2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
sgl_result_t sgl_simd_rgb2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
sgl_result_t sgl_simd_yuv2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
sgl_result_t sgl_simd_yuv2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src, sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst, sgl_fourcc_t dst_format,
    sgl_int32_t width, sgl_int32_t height);
#endif  /* !SGL_CFG_HAS_SIMD */


/*******************************************************************
 *                          Queue
 *******************************************************************/
sgl_queue_t *sgl_queue_create(sgl_size_t capacity);
void sgl_queue_destroy(sgl_queue_t **queue);
sgl_result_t sgl_queue_copy(sgl_queue_t *SGL_RESTRICT dst, const sgl_queue_t *SGL_RESTRICT src);
sgl_result_t sgl_queue_unsafe_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data);
sgl_result_t sgl_queue_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data);
void *sgl_queue_dequeue(sgl_queue_t *queue);
void *sgl_queue_peek(sgl_queue_t *queue);
sgl_result_t sgl_queue_is_empty(const sgl_queue_t *queue);
sgl_result_t sgl_queue_is_full(const sgl_queue_t *queue);
sgl_size_t sgl_queue_get_capacity(const sgl_queue_t *queue);
sgl_size_t sgl_queue_get_count(const sgl_queue_t *queue);


/*******************************************************************
 *                          Threadpool
 *******************************************************************/
#if defined(SGL_CFG_HAS_THREAD)
sgl_threadpool_t *sgl_threadpool_create(sgl_size_t num_threads, sgl_size_t max_routine_lists, const char *base_name);
sgl_result_t sgl_threadpool_destroy(sgl_threadpool_t *pool);
sgl_size_t sgl_threadpool_get_num_threads(const sgl_threadpool_t *pool);
sgl_result_t sgl_threadpool_attach_routine(sgl_threadpool_t *SGL_RESTRICT pool, sgl_threadpool_routine_t routine, sgl_queue_t *SGL_RESTRICT operations, void *SGL_RESTRICT cookie);
/* Consumes operation entries; the caller retains ownership of the queue. */
sgl_result_t sgl_threadpool_attach_routine_consuming(sgl_threadpool_t *SGL_RESTRICT pool, sgl_threadpool_routine_t routine, sgl_queue_t *SGL_RESTRICT operations, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

#if defined(__cplusplus)
}
#endif

#endif  /* !SGL_CORE_H_ */

/* End Of File */
