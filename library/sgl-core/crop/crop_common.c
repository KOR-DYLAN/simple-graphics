/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "crop_common.h"

#include "../threadpool/parallel_for.h"

#if defined(SGL_CFG_HAS_THREAD)
#define SGL_CROP_MIN_PARALLEL_BYTES  ((sgl_uint64_t)262144U)
#endif

/*
 * src points at the crop origin. src_stride advances through the original
 * image, while dst_stride is both the destination stride and copied row size.
 */
typedef struct {
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_size_t src_stride;
    sgl_size_t dst_stride;
    sgl_crop_copy_row_t copy_row;
} sgl_crop_data_t;

/* Prove width * height * bpp is representable before computing byte offsets. */
static sgl_bool_t sgl_crop_buffer_size_is_valid(
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t bpp)
{
    sgl_bool_t result = SGL_FALSE;

    if ((width > 0) && (height > 0) && (bpp > 0)) {
        sgl_size_t width_size;
        sgl_size_t height_size;
        sgl_size_t bpp_size;

        width_size = (sgl_size_t)(sgl_uint32_t)width;
        height_size = (sgl_size_t)(sgl_uint32_t)height;
        bpp_size = (sgl_size_t)(sgl_uint32_t)bpp;
        if (width_size <= (SGL_SIZE_MAX / height_size)) {
            sgl_size_t pixels;

            pixels = width_size * height_size;
            if (pixels <= (SGL_SIZE_MAX / bpp_size)) {
                result = SGL_TRUE;
            }
        }
    }

    return result;
}

static sgl_bool_t sgl_crop_has_valid_origin(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_bool_t result = SGL_FALSE;

    if ((src != SGL_NULL) && (dst != SGL_NULL) &&
        (x >= 0) && (y >= 0)) {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_crop_has_valid_shape(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp)
{
    sgl_bool_t result = SGL_FALSE;

    if ((sgl_crop_buffer_size_is_valid(
             src_width, src_height, bpp) == SGL_TRUE) &&
        (sgl_crop_buffer_size_is_valid(
             crop_width, crop_height, bpp) == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

/* Validate dimensions separately so boundary subtraction cannot overflow. */
sgl_result_t sgl_crop_validate(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((sgl_crop_has_valid_origin(src, dst, x, y) != SGL_TRUE) ||
        (sgl_crop_has_valid_shape(
            src_width, src_height, crop_width, crop_height, bpp) !=
         SGL_TRUE)) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }
    else if (((src_width - x) < crop_width) ||
             ((src_height - y) < crop_height)) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }
    else {
        result = SGL_SUCCESS;
    }

    return result;
}

static void sgl_crop_run_range(
    const sgl_parallel_range_t *range,
    const sgl_crop_data_t *data)
{
    sgl_int32_t row;
    sgl_int32_t end;
    const sgl_uint8_t *src_row;
    sgl_uint8_t *dst_row;

    end = range->start + range->count;
    src_row = &data->src[
        (sgl_size_t)(sgl_uint32_t)range->start * data->src_stride];
    dst_row = &data->dst[
        (sgl_size_t)(sgl_uint32_t)range->start * data->dst_stride];
    for (row = range->start; row < end; ++row) {
        /* Each row is independent and writes one contiguous destination row. */
        data->copy_row(src_row, dst_row, data->dst_stride);
        src_row = &src_row[data->src_stride];
        dst_row = &dst_row[data->dst_stride];
    }
}

/* cppcheck-suppress constParameterCallback */
static void sgl_crop_routine(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie)
{
    const sgl_parallel_range_t *range;
    const sgl_crop_data_t *data;

    range = sgl_parallel_range_from_memory(current);
    /* SGL-MEM-DEV-001: typed conversion from threadpool cookie storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    data = (const sgl_crop_data_t *)cookie;
    sgl_crop_run_range(range, data);
}

static sgl_bool_t sgl_crop_should_parallel(
    const sgl_threadpool_t *pool,
    sgl_size_t row_bytes,
    sgl_int32_t crop_height)
{
    sgl_bool_t result = SGL_FALSE;

#if defined(SGL_CFG_HAS_THREAD)
    /* Thread setup is only amortized for crops of at least 256 KiB. */
    if ((pool != SGL_NULL) &&
        (sgl_threadpool_get_num_threads(pool) > 1U) &&
        (((sgl_uint64_t)row_bytes *
          (sgl_uint64_t)(sgl_uint32_t)crop_height) >=
         SGL_CROP_MIN_PARALLEL_BYTES)) {
        result = SGL_TRUE;
    }
#else
    (void)pool;
    (void)row_bytes;
    (void)crop_height;
#endif  /* SGL_CFG_HAS_THREAD */

    return result;
}

static sgl_crop_data_t sgl_crop_make_data(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t src_width,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t bpp,
    sgl_crop_copy_row_t copy_row)
{
    sgl_crop_data_t result;
    sgl_size_t pixel_bytes;
    sgl_size_t src_offset;

    pixel_bytes = (sgl_size_t)(sgl_uint32_t)bpp;
    result.src_stride =
        (sgl_size_t)(sgl_uint32_t)src_width * pixel_bytes;
    result.dst_stride =
        (sgl_size_t)(sgl_uint32_t)crop_width * pixel_bytes;
    /* Convert the two-dimensional crop origin to its source byte offset. */
    src_offset =
        (((sgl_size_t)(sgl_uint32_t)y *
          (sgl_size_t)(sgl_uint32_t)src_width) +
         (sgl_size_t)(sgl_uint32_t)x) * pixel_bytes;
    result.src = &src[src_offset];
    result.dst = dst;
    result.copy_row = copy_row;

    return result;
}

sgl_result_t sgl_crop_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t src_width,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp,
    sgl_crop_copy_row_t copy_row)
{
    sgl_crop_data_t data;
    sgl_parallel_range_t range;
    sgl_result_t result = SGL_SUCCESS;

    data = sgl_crop_make_data(
        src,
        dst,
        src_width,
        x,
        y,
        crop_width,
        bpp,
        copy_row);
    if (data.src_stride == data.dst_stride) {
        /*
         * A full-width crop has no gap between selected source rows.  Copying
         * the complete region once exposes one contiguous transfer to the
         * memory backend and removes one function call per image row.
         */
        data.copy_row(
            data.src,
            data.dst,
            data.dst_stride *
                (sgl_size_t)(sgl_uint32_t)crop_height);
    }
    else if (sgl_crop_should_parallel(
            pool, data.dst_stride, crop_height) == SGL_TRUE) {
        result = sgl_parallel_for(
            pool,
            crop_height,
            1,
            sgl_crop_routine,
            (void *)&data);
    }
    else {
        range.start = 0;
        range.count = crop_height;
        sgl_crop_run_range(&range, &data);
    }

    return result;
}
