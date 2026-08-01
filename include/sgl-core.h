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

/**
 * @file sgl-core.h
 * @brief Public API for memory, image processing, queues, and worker threads.
 *
 * Include this header for the complete SGL runtime API. Image buffers are
 * tightly packed and all dimensions are expressed in pixels unless stated
 * otherwise. Source and destination ranges passed through SGL_RESTRICT must
 * not overlap.
 */

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

/** @defgroup sgl_utility Utility macros
 *  @brief Common constants and source-level helpers.
 *  @{
 */
/** Explicitly marks an intentionally unused parameter. */
#define SGL_UNUSED_PARAM(p)                         SGL_UNUSED(p)
/** Divides positive integer @p n by positive integer @p d, rounding upward. */
#define SGL_DIV_ROUNDUP(n, d)                       (((n) + (d) - 1) / (d))
/** Releases an SGL allocation and sets the pointer lvalue to #SGL_NULL. */
#define SGL_SAFE_FREE(p)                            if ((p) != SGL_NULL) { sgl_free((p)); (p) = SGL_NULL; }
/** Default maximum routine-list count accepted by sgl_threadpool_create(). */
#define SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS    (4U)
/** Default row-group size used by scalar worker routines. */
#define SGL_GENERIC_BULK_SIZE                       (4)
/** Default row-group size used by SIMD worker routines. */
#define SGL_SIMD_BULK_SIZE                          (8)
/** Four bytes per pixel. */
#define SGL_BPP32                                   (4)
/** Three bytes per pixel. */
#define SGL_BPP24                                   (3)
/** Two bytes per pixel. */
#define SGL_BPP16                                   (2)
/** One byte per pixel. */
#define SGL_BPP8                                    (1)
/** @} */


/** @defgroup sgl_types Result and opaque object types
 *  @brief Types shared by the public API.
 *  @{
 */
/** Result codes returned by SGL operations. */
typedef enum {
    SGL_SUCCESS,                       /**< Operation completed successfully. */
    SGL_FAILURE,                       /**< Operation failed without a more specific code. */
    SGL_ERROR_INVALID_ARGUMENTS,       /**< One or more arguments violate the API contract. */
    SGL_ERROR_MEMORY_ALLOCATION,       /**< The configured SGL memory pool could not satisfy an allocation. */
    SGL_ERROR_MISSMATCHED_CAPACITY,    /**< Destination queue capacity is smaller than the source capacity. */
    SGL_ERROR_NOT_SUPPORTED,           /**< Requested feature is unavailable in this build. */
    SGL_QUEUE_IS_EMPTY,                /**< Queue contains no entries. */
    SGL_QUEUE_IS_NOT_EMPTY,            /**< Queue contains at least one entry. */
    SGL_QUEUE_IS_FULL,                 /**< Queue has reached its fixed capacity. */
    SGL_QUEUE_IS_NOT_FULL,             /**< Queue can accept another entry. */
} sgl_result_t;

/** Opaque nearest-neighbor resize lookup table. */
typedef struct sgl_nearest_neighbor_lookup_table    sgl_nearest_neighbor_lookup_t;
/** Opaque bilinear resize lookup table. */
typedef struct sgl_bilinear_lookup_table            sgl_bilinear_lookup_t;
/** Opaque bicubic resize lookup table. */
typedef struct sgl_bicubic_lookup_table             sgl_bicubic_lookup_t;
/** Opaque nearest-neighbor rotation lookup table. */
typedef struct sgl_rotate_lookup_table              sgl_rotate_lookup_t;
/** Opaque fixed-capacity queue. */
typedef struct sgl_queue                            sgl_queue_t;
/** Opaque synchronous worker-thread pool. */
typedef struct sgl_threadpool                       sgl_threadpool_t;
/**
 * @brief Processes one queue entry submitted to a thread pool.
 * @param[in,out] current Entry dequeued from the operation queue.
 * @param[in,out] cookie Caller context passed to the attach function.
 *
 * The callback can run on the submitting thread or on any worker thread.
 * Concurrent invocations must therefore synchronize shared cookie state.
 */
typedef void (*sgl_threadpool_routine_t)(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie);
/** @} */


/** @defgroup sgl_memory_operations Memory operations
 *  @brief Runtime-independent byte copy and fill functions.
 *  @{
 */
/**
 * @brief Copies bytes between non-overlapping memory ranges.
 * @param[out] destination Writable range containing at least @p size bytes.
 * @param[in] source Readable range containing at least @p size bytes.
 * @param[in] size Number of bytes to copy.
 * @return @p destination.
 * @pre The two ranges do not overlap. Use with a zero size is valid.
 */
void *sgl_memcpy(void *SGL_RESTRICT destination,
                 const void *SGL_RESTRICT source,
                 sgl_size_t size);
/**
 * @brief Fills a memory range with one byte value.
 * @param[out] destination Writable range containing at least @p size bytes.
 * @param[in] value Value whose low eight bits are stored in each byte.
 * @param[in] size Number of bytes to set.
 * @return @p destination.
 */
void *sgl_memset(void *destination, sgl_int32_t value, sgl_size_t size);
/** @} */


/** @defgroup sgl_memory_pool Memory pool
 *  @brief Allocation from one caller-owned process-wide memory region.
 *
 * SGL does not fall back to the C runtime heap. Initialize the pool before
 * creating LUTs, queues, thread pools, or invoking an operation that needs
 * temporary storage. Initialization and deinitialization are not concurrent
 * operations; allocation and release are synchronized after initialization.
 *  @{
 */
/**
 * @brief Registers the process-wide SGL memory pool.
 * @param[in,out] memory Caller-owned storage retained until deinitialization.
 * @param[in] size Size of @p memory in bytes, including allocator metadata.
 * @retval SGL_SUCCESS Pool initialized successfully.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid storage, insufficient size, or
 *         an already initialized pool.
 * @note The implementation aligns the usable range inside the supplied buffer.
 */
sgl_result_t sgl_memory_pool_initialize(void *memory, sgl_size_t size);
/**
 * @brief Detaches the caller-owned memory pool.
 * @retval SGL_SUCCESS No live allocations remain and the pool was detached.
 * @retval SGL_FAILURE At least one allocation is still live.
 * @retval SGL_ERROR_INVALID_ARGUMENTS The pool is not initialized.
 */
sgl_result_t sgl_memory_pool_deinitialize(void);
/**
 * @brief Allocates an aligned block from the SGL memory pool.
 * @param[in] size Requested size in bytes.
 * @return Allocated memory, or #SGL_NULL for zero size, an uninitialized pool,
 *         overflow, or insufficient free space.
 */
void *sgl_malloc(sgl_size_t size);
/**
 * @brief Allocates and zero-initializes an array from the SGL memory pool.
 * @param[in] count Number of elements.
 * @param[in] size Size of one element in bytes.
 * @return Zero-filled memory, or #SGL_NULL when the request cannot be served.
 */
void *sgl_calloc(sgl_size_t count, sgl_size_t size);
/**
 * @brief Releases a block allocated by sgl_malloc() or sgl_calloc().
 * @param[in,out] memory Allocation to release; #SGL_NULL is accepted.
 * @note Pointers outside the active pool are ignored. Each live allocation
 *       must be released exactly once; a previously released pointer is invalid.
 */
void sgl_free(void *memory);
/** @} */


/** @defgroup sgl_resize Resize
 *  @brief Nearest-neighbor, bilinear, and bicubic packed-image resize.
 *
 * Buffers use tightly packed rows of `width * bpp` bytes. A #SGL_NULL pool
 * selects synchronous single-thread execution. A reusable LUT avoids geometry
 * allocation on repeated calls with identical dimensions; a #SGL_NULL or
 * dimension-mismatched LUT causes the operation to allocate a temporary LUT.
 *  @{
 */
/**
 * @brief Creates a nearest-neighbor lookup table for one resize geometry.
 * @param[in] d_width Destination width; must be at least 2.
 * @param[in] d_height Destination height; must be at least 2.
 * @param[in] s_width Source width; must be at least 2.
 * @param[in] s_height Source height; must be at least 2.
 * @return New lookup table, or #SGL_NULL when allocation fails.
 */
sgl_nearest_neighbor_lookup_t *sgl_generic_create_nearest_neighbor_lut(
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height);
/** @brief Creates a bilinear lookup table. @copydetails sgl_generic_create_nearest_neighbor_lut */
sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height);
/** @brief Creates a bicubic lookup table. @copydetails sgl_generic_create_nearest_neighbor_lut */
sgl_bicubic_lookup_t *sgl_generic_create_bicubic_lut(
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height);

/** @brief Destroys a nearest-neighbor LUT. @param[in] lut LUT or #SGL_NULL. */
void sgl_generic_destroy_nearest_neighbor_lut(sgl_nearest_neighbor_lookup_t *lut);
/** @brief Destroys a bilinear LUT. @param[in] lut LUT or #SGL_NULL. */
void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut);
/** @brief Destroys a bicubic LUT. @param[in] lut LUT or #SGL_NULL. */
void sgl_generic_destroy_bicubic_lut(sgl_bicubic_lookup_t *lut);

/**
 * @brief Resizes a packed image using nearest-neighbor sampling.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] ext_lut Reusable LUT for these dimensions, or #SGL_NULL.
 * @param[out] dst Buffer of at least `d_width * d_height * bpp` bytes.
 * @param[in] d_width Destination width in pixels.
 * @param[in] d_height Destination height in pixels.
 * @param[in] src Buffer of at least `s_width * s_height * bpp` bytes.
 * @param[in] s_width Source width in pixels.
 * @param[in] s_height Source height in pixels.
 * @param[in] bpp Positive number of bytes per packed pixel.
 * @retval SGL_SUCCESS Resize completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS A pointer, dimension, or @p bpp is invalid.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Temporary work storage was unavailable.
 * @retval SGL_ERROR_NOT_SUPPORTED A non-NULL pool was supplied to a build
 *         without thread support.
 * @pre Source and destination buffers do not overlap. Dimensions are at least
 *      2 when source and destination sizes differ.
 */
sgl_result_t sgl_generic_resize_nearest(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp);

/** @brief Resizes a packed image using bilinear interpolation. @copydetails sgl_generic_resize_nearest */
sgl_result_t sgl_generic_resize_bilinear(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp);

/** @brief Resizes a packed image using bicubic interpolation. @copydetails sgl_generic_resize_nearest */
sgl_result_t sgl_generic_resize_bicubic(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp);

#if defined(SGL_CFG_HAS_SIMD)
/** @brief SIMD nearest-neighbor resize. @copydetails sgl_generic_resize_nearest */
sgl_result_t sgl_simd_resize_nearest(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp);

/** @brief SIMD bilinear resize. @copydetails sgl_generic_resize_bilinear */
sgl_result_t sgl_simd_resize_bilinear(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp);
/** @brief SIMD bicubic resize. @copydetails sgl_generic_resize_bicubic */
sgl_result_t sgl_simd_resize_bicubic(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp);
#endif  /* SGL_CFG_HAS_SIMD */
/** @} */

/** @defgroup sgl_crop Crop
 *  @brief Rectangular crop of a tightly packed image.
 *  @{
 */
/**
 * @brief Copies a rectangular source region to a compact destination image.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] src Buffer of at least `src_width * src_height * bpp` bytes.
 * @param[in] src_width Source width in pixels.
 * @param[in] src_height Source height in pixels.
 * @param[out] dst Buffer of at least `crop_width * crop_height * bpp` bytes.
 * @param[in] x Zero-based source x coordinate of the crop origin.
 * @param[in] y Zero-based source y coordinate of the crop origin.
 * @param[in] crop_width Positive crop width in pixels.
 * @param[in] crop_height Positive crop height in pixels.
 * @param[in] bpp Positive number of bytes per packed pixel.
 * @retval SGL_SUCCESS Crop completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid buffer, geometry, or byte count.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Parallel work storage was unavailable.
 * @pre The crop rectangle lies completely inside the source and the source and
 *      destination buffers do not overlap.
 */
sgl_result_t sgl_generic_crop(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp);
#if defined(SGL_CFG_HAS_SIMD)
/** @brief SIMD crop implementation. @copydetails sgl_generic_crop */
sgl_result_t sgl_simd_crop(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp);
#endif  /* SGL_CFG_HAS_SIMD */
/** @} */

/** @defgroup sgl_rotate Rotate
 *  @brief Nearest-neighbor rotation into a caller-owned bounding-box buffer.
 *
 * Angles are clockwise degrees and are normalized modulo 360. Pixels in the
 * destination bounding box that map outside the source are filled with zero.
 *  @{
 */
/**
 * @brief Calculates the destination bounding-box dimensions for a rotation.
 * @param[in] src_width Positive source width in pixels.
 * @param[in] src_height Positive source height in pixels.
 * @param[in] angle Clockwise rotation angle in degrees.
 * @param[out] dst_width Calculated destination width.
 * @param[out] dst_height Calculated destination height.
 * @retval SGL_SUCCESS Dimensions calculated successfully.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid dimensions, output pointer, NaN,
 *         unsupported angle magnitude, or overflowing result.
 */
sgl_result_t sgl_get_rotated_dimensions(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_float64_t angle,
    sgl_int32_t *dst_width,
    sgl_int32_t *dst_height);
/**
 * @brief Calculates the required destination buffer size for a rotation.
 * @param[in] src_width Positive source width in pixels.
 * @param[in] src_height Positive source height in pixels.
 * @param[in] bpp Positive number of bytes per packed pixel.
 * @param[in] angle Clockwise rotation angle in degrees.
 * @return Required byte count, or 0 for invalid or overflowing input.
 */
sgl_int32_t sgl_get_rotated_size(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t bpp,
    sgl_float64_t angle);
/**
 * @brief Creates a reusable rotation mapping table.
 * @param[in] src_width Positive source width in pixels.
 * @param[in] src_height Positive source height in pixels.
 * @param[in] angle Clockwise rotation angle in degrees.
 * @return New LUT, or #SGL_NULL for invalid geometry or allocation failure.
 * @note A LUT is independent of @p bpp and is valid only for the exact source
 *       dimensions and normalized angle used during creation.
 */
sgl_rotate_lookup_t *sgl_generic_create_rotate_lut(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_float64_t angle);
/** @brief Destroys a rotation LUT. @param[in] lut LUT or #SGL_NULL. */
void sgl_generic_destroy_rotate_lut(sgl_rotate_lookup_t *lut);

/**
 * @brief Rotates a tightly packed image using nearest-neighbor sampling.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] ext_lut Matching reusable rotation LUT, or #SGL_NULL.
 * @param[in] src Buffer of at least `src_width * src_height * bpp` bytes.
 * @param[in] src_width Positive source width in pixels.
 * @param[in] src_height Positive source height in pixels.
 * @param[out] dst Caller-owned buffer sized by sgl_get_rotated_size().
 * @param[in] bpp Positive number of bytes per packed pixel.
 * @param[in] angle Clockwise rotation angle in degrees.
 * @retval SGL_SUCCESS Rotation completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid input or a LUT created for
 *         different geometry.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Parallel work storage was unavailable.
 * @pre Source and destination buffers do not overlap.
 */
sgl_result_t sgl_generic_rotate(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_rotate_lookup_t *SGL_RESTRICT ext_lut,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp,
    sgl_float64_t angle);
#if defined(SGL_CFG_HAS_SIMD)
/** @brief SIMD rotation implementation. @copydetails sgl_generic_rotate */
sgl_result_t sgl_simd_rotate(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_rotate_lookup_t *SGL_RESTRICT ext_lut,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp,
    sgl_float64_t angle);
#endif  /* SGL_CFG_HAS_SIMD */
/** @} */

/** @defgroup sgl_convert Pixel-format conversion
 *  @brief Conversion between the RGB and YUV formats in sgl-fourcc.h.
 *
 * Buffers are tightly packed according to their FourCC layout and support odd
 * image dimensions by rounding chroma dimensions upward. RGB/YUV conversion
 * uses fixed-point color equations. Alpha is preserved between alpha-bearing
 * RGB formats, synthesized as 255 when absent, and discarded by YUV formats.
 * A #SGL_NULL pool selects single-thread execution.
 *  @{
 */
/**
 * @brief Converts any supported RGB layout to any supported YUV layout.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] src Source image in @p src_format.
 * @param[in] src_format One of the RGB FourCC values in sgl-fourcc.h.
 * @param[out] dst Destination image in @p dst_format.
 * @param[in] dst_format One of the YUV FourCC values in sgl-fourcc.h.
 * @param[in] width Positive image width in pixels.
 * @param[in] height Positive image height in pixels.
 * @retval SGL_SUCCESS Conversion completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid buffer, dimensions, or size.
 * @retval SGL_ERROR_NOT_SUPPORTED FourCC category or build feature unsupported.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Parallel work storage was unavailable.
 * @pre Source and destination buffers do not overlap.
 * @see sgl_fourcc_formats
 */
sgl_result_t sgl_generic_rgb2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
/**
 * @brief Converts between any two supported RGB layouts.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] src Source image in @p src_format.
 * @param[in] src_format Any supported RGB FourCC value.
 * @param[out] dst Destination image in @p dst_format.
 * @param[in] dst_format Any supported RGB FourCC value.
 * @param[in] width Positive image width in pixels.
 * @param[in] height Positive image height in pixels.
 * @retval SGL_SUCCESS Conversion completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid buffer, dimensions, or size.
 * @retval SGL_ERROR_NOT_SUPPORTED A FourCC is not an RGB format.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Parallel work storage was unavailable.
 * @pre Source and destination buffers do not overlap.
 */
sgl_result_t sgl_generic_rgb2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
/**
 * @brief Converts any supported YUV layout to any supported RGB layout.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] src Source image in @p src_format.
 * @param[in] src_format Any supported YUV FourCC value.
 * @param[out] dst Destination image in @p dst_format.
 * @param[in] dst_format Any supported RGB FourCC value.
 * @param[in] width Positive image width in pixels.
 * @param[in] height Positive image height in pixels.
 * @retval SGL_SUCCESS Conversion completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid buffer, dimensions, or size.
 * @retval SGL_ERROR_NOT_SUPPORTED Invalid source or destination category.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Parallel work storage was unavailable.
 * @pre Source and destination buffers do not overlap.
 */
sgl_result_t sgl_generic_yuv2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
/**
 * @brief Converts between any two supported YUV layouts.
 * @param[in] pool Worker pool, or #SGL_NULL for single-thread execution.
 * @param[in] src Source image in @p src_format.
 * @param[in] src_format Any supported YUV FourCC value.
 * @param[out] dst Destination image in @p dst_format.
 * @param[in] dst_format Any supported YUV FourCC value.
 * @param[in] width Positive image width in pixels.
 * @param[in] height Positive image height in pixels.
 * @retval SGL_SUCCESS Conversion completed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS Invalid buffer, dimensions, or size.
 * @retval SGL_ERROR_NOT_SUPPORTED A FourCC is not a YUV format.
 * @retval SGL_ERROR_MEMORY_ALLOCATION Parallel work storage was unavailable.
 * @pre Source and destination buffers do not overlap.
 */
sgl_result_t sgl_generic_yuv2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
#if defined(SGL_CFG_HAS_SIMD)
/** @brief SIMD RGB-to-YUV conversion. @copydetails sgl_generic_rgb2yuv */
sgl_result_t sgl_simd_rgb2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
/** @brief SIMD RGB-to-RGB conversion. @copydetails sgl_generic_rgb2rgb */
sgl_result_t sgl_simd_rgb2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
/** @brief SIMD YUV-to-RGB conversion. @copydetails sgl_generic_yuv2rgb */
sgl_result_t sgl_simd_yuv2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
/** @brief SIMD YUV-to-YUV conversion. @copydetails sgl_generic_yuv2yuv */
sgl_result_t sgl_simd_yuv2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
#endif  /* SGL_CFG_HAS_SIMD */
/** @} */

/** @defgroup sgl_queue Queue
 *  @brief Fixed-capacity FIFO queue of non-owning object pointers.
 *
 * The queue owns only its pointer ring. Enqueued objects remain owned by the
 * caller. Public enqueue, dequeue, and peek operations synchronize access;
 * status and count accessors are snapshots and do not acquire the queue lock.
 *  @{
 */
/**
 * @brief Creates an empty queue.
 * @param[in] capacity Positive maximum number of entries.
 * @return New queue, or #SGL_NULL for invalid capacity or allocation failure.
 */
sgl_queue_t *sgl_queue_create(sgl_size_t capacity);
/**
 * @brief Destroys a queue and clears the caller's handle.
 * @param[in,out] queue Address of a queue handle; either pointer may be #SGL_NULL.
 */
void sgl_queue_destroy(sgl_queue_t **queue);
/**
 * @brief Copies queue state and entries without transferring object ownership.
 * @param[out] dst Destination queue.
 * @param[in] src Source queue.
 * @retval SGL_SUCCESS Queue state copied.
 * @retval SGL_ERROR_MISSMATCHED_CAPACITY Destination capacity is too small.
 * @retval SGL_ERROR_INVALID_ARGUMENTS A queue pointer is #SGL_NULL.
 * @warning The caller must exclude concurrent access to both queues.
 */
sgl_result_t sgl_queue_copy(sgl_queue_t *SGL_RESTRICT dst, const sgl_queue_t *SGL_RESTRICT src);
/**
 * @brief Enqueues an object pointer without locking the queue.
 * @param[in,out] queue Destination queue.
 * @param[in] data Non-NULL object pointer; ownership is retained by the caller.
 * @retval SGL_SUCCESS Entry enqueued.
 * @retval SGL_QUEUE_IS_FULL Queue has no free slot.
 * @retval SGL_ERROR_INVALID_ARGUMENTS A pointer is #SGL_NULL.
 * @warning Use only while the caller has exclusive access to @p queue.
 */
sgl_result_t sgl_queue_unsafe_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data);
/**
 * @brief Enqueues an object pointer while holding the queue lock.
 * @param[in,out] queue Destination queue.
 * @param[in] data Non-NULL object pointer; ownership is retained by the caller.
 * @retval SGL_SUCCESS Entry enqueued.
 * @retval SGL_QUEUE_IS_FULL Queue has no free slot.
 * @retval SGL_ERROR_INVALID_ARGUMENTS A pointer is #SGL_NULL.
 */
sgl_result_t sgl_queue_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data);
/**
 * @brief Removes and returns the oldest queue entry.
 * @param[in,out] queue Queue to modify.
 * @return Dequeued object pointer, or #SGL_NULL for a NULL or empty queue.
 */
void *sgl_queue_dequeue(sgl_queue_t *queue);
/**
 * @brief Returns the oldest queue entry without removing it.
 * @param[in] queue Queue to inspect.
 * @return Oldest object pointer, or #SGL_NULL for a NULL or empty queue.
 */
void *sgl_queue_peek(sgl_queue_t *queue);
/**
 * @brief Tests whether a valid queue is empty.
 * @param[in] queue Queue to inspect; must not be #SGL_NULL.
 * @retval SGL_QUEUE_IS_EMPTY Queue contains no entries.
 * @retval SGL_QUEUE_IS_NOT_EMPTY Queue contains entries.
 */
sgl_result_t sgl_queue_is_empty(const sgl_queue_t *queue);
/**
 * @brief Tests whether a valid queue is full.
 * @param[in] queue Queue to inspect; must not be #SGL_NULL.
 * @retval SGL_QUEUE_IS_FULL Queue reached capacity.
 * @retval SGL_QUEUE_IS_NOT_FULL Queue has a free slot.
 */
sgl_result_t sgl_queue_is_full(const sgl_queue_t *queue);
/** @brief Returns queue capacity. @param[in] queue Queue or #SGL_NULL. @return Capacity, or 0 for #SGL_NULL. */
sgl_size_t sgl_queue_get_capacity(const sgl_queue_t *queue);
/** @brief Returns the current entry count. @param[in] queue Queue or #SGL_NULL. @return Entry count, or 0 for #SGL_NULL. */
sgl_size_t sgl_queue_get_count(const sgl_queue_t *queue);
/** @} */


/** @defgroup sgl_threadpool Thread pool
 *  @brief Synchronous parallel processing of queue entries.
 *
 * Attach calls block until every entry has been processed. The submitting
 * thread participates in execution, so @p num_threads is the maximum number of
 * background workers rather than a total concurrency count.
 *  @{
 */
#if defined(SGL_CFG_HAS_THREAD)
/**
 * @brief Creates a worker-thread pool.
 * @param[in] num_threads Positive number of background worker threads.
 * @param[in] max_routine_lists Positive submission-capacity contract; use
 *        #SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS for the default.
 * @param[in] base_name Optional caller-owned thread-name prefix retained for
 *        the lifetime of the pool.
 * @return New thread pool, or #SGL_NULL for invalid input or allocation failure.
 */
sgl_threadpool_t *sgl_threadpool_create(
    sgl_size_t num_threads,
    sgl_size_t max_routine_lists,
    const char *base_name);
/**
 * @brief Stops workers and destroys a thread pool.
 * @param[in] pool Pool to destroy; must not be #SGL_NULL.
 * @retval SGL_SUCCESS Pool destroyed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS @p pool is #SGL_NULL.
 */
sgl_result_t sgl_threadpool_destroy(sgl_threadpool_t *pool);
/**
 * @brief Returns the number of background workers.
 * @param[in] pool Pool or #SGL_NULL.
 * @return Worker count, or 0 for #SGL_NULL.
 */
sgl_size_t sgl_threadpool_get_num_threads(const sgl_threadpool_t *pool);
/**
 * @brief Processes all queued operations and restores their queue entries.
 * @param[in,out] pool Worker pool.
 * @param[in] routine Callback invoked once for each operation.
 * @param[in,out] operations Queue of non-NULL operation pointers.
 * @param[in,out] cookie Optional callback context.
 * @retval SGL_SUCCESS Every operation completed and the queue was restored.
 * @retval SGL_ERROR_INVALID_ARGUMENTS A required argument is #SGL_NULL or the
 *         pool is shutting down.
 * @retval SGL_ERROR_MEMORY_ALLOCATION The temporary completion queue could not
 *         be allocated.
 * @note Queue entry order after parallel execution is completion order and may
 *       differ from the original enqueue order.
 */
sgl_result_t sgl_threadpool_attach_routine(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_threadpool_routine_t routine,
    sgl_queue_t *SGL_RESTRICT operations,
    void *SGL_RESTRICT cookie);
/**
 * @brief Processes and removes every operation entry from the queue.
 * @param[in,out] pool Worker pool.
 * @param[in] routine Callback invoked once for each operation.
 * @param[in,out] operations Queue that is empty when the call succeeds.
 * @param[in,out] cookie Optional callback context.
 * @retval SGL_SUCCESS Every operation completed and was consumed.
 * @retval SGL_ERROR_INVALID_ARGUMENTS A required argument is #SGL_NULL or the
 *         pool is shutting down.
 * @note The caller retains ownership of the empty queue and all operation
 *       objects. This variant avoids allocating a completion queue.
 */
sgl_result_t sgl_threadpool_attach_routine_consuming(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_threadpool_routine_t routine,
    sgl_queue_t *SGL_RESTRICT operations,
    void *SGL_RESTRICT cookie);
#endif  /* SGL_CFG_HAS_THREAD */
/** @} */

#if defined(__cplusplus)
}
#endif

#endif  /* !SGL_CORE_H_ */

/* End Of File */
