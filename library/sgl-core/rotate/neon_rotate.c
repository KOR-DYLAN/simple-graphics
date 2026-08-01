/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <arm_neon.h>

#include "rotate_common.h"

#include "../threadpool/parallel_for.h"

#define SGL_NEON_ROTATE_VECTOR_BYTES         (16U)
#define SGL_NEON_ROTATE_RGBA_TILE            (4)
#define SGL_NEON_ROTATE_90_Q16               (5898240)
#define SGL_NEON_ROTATE_180_Q16              (11796480)
#define SGL_NEON_ROTATE_270_Q16              (17694720)
#define SGL_NEON_ROTATE_MIN_PARALLEL_PIXELS  ((sgl_size_t)65536U)

typedef void (*sgl_neon_rotate_range_kernel_t)(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count);

typedef struct {
    const sgl_rotate_context_t *context;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    const sgl_rotate_lookup_t *lut;
    sgl_neon_rotate_range_kernel_t kernel;
} sgl_neon_rotate_data_t;

typedef struct {
    uint32x4_t column0;
    uint32x4_t column1;
    uint32x4_t column2;
    uint32x4_t column3;
} sgl_neon_rotate_rgba_tile_t;

typedef enum {
    SGL_NEON_ROTATE_SCHEDULE_STATIC = 0,
    SGL_NEON_ROTATE_SCHEDULE_DYNAMIC
} sgl_neon_rotate_schedule_t;

static void sgl_neon_rotate_run_range(
    const sgl_neon_rotate_data_t *data,
    const sgl_parallel_range_t *range)
{
    data->kernel(
        data->context,
        data->src,
        data->dst,
        data->lut,
        range->start,
        range->count);
}

/* cppcheck-suppress constParameterCallback */
static void sgl_neon_rotate_routine(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie)
{
    const sgl_parallel_range_t *range;
    const sgl_neon_rotate_data_t *data;

    range = sgl_parallel_range_from_memory(current);
    /* SGL-MEM-DEV-001: typed conversion from threadpool cookie storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    data = (const sgl_neon_rotate_data_t *)cookie;
    sgl_neon_rotate_run_range(data, range);
}

static sgl_bool_t sgl_neon_rotate_should_parallel(
    const sgl_threadpool_t *pool,
    const sgl_rotate_context_t *context)
{
    sgl_size_t pixels;
    sgl_bool_t result = SGL_FALSE;

#if defined(SGL_CFG_HAS_THREAD)
    pixels = (sgl_size_t)(sgl_uint32_t)context->src_width *
        (sgl_size_t)(sgl_uint32_t)context->src_height;
    if ((pool != SGL_NULL) &&
        (sgl_threadpool_get_num_threads(pool) > 1U) &&
        (pixels >= SGL_NEON_ROTATE_MIN_PARALLEL_PIXELS)) {
        result = SGL_TRUE;
    }
#else
    (void)pool;
    (void)context;
    pixels = 0U;
#endif
    (void)pixels;

    return result;
}

static sgl_result_t sgl_neon_rotate_execute(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_neon_rotate_data_t *data,
    sgl_int32_t item_count,
    sgl_int32_t item_alignment,
    sgl_neon_rotate_schedule_t schedule)
{
    sgl_result_t result = SGL_SUCCESS;

    if (sgl_neon_rotate_should_parallel(
            pool, data->context) == SGL_TRUE) {
        if (schedule == SGL_NEON_ROTATE_SCHEDULE_DYNAMIC) {
            result = sgl_parallel_for(
                pool,
                item_count,
                item_alignment,
                sgl_neon_rotate_routine,
                (void *)data);
        }
        else {
            result = sgl_parallel_for_static(
                pool,
                item_count,
                item_alignment,
                sgl_neon_rotate_routine,
                (void *)data);
        }
    }
    else {
        data->kernel(
            data->context,
            data->src,
            data->dst,
            data->lut,
            0,
            item_count);
    }

    return result;
}

static sgl_neon_rotate_data_t sgl_neon_rotate_make_data(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_neon_rotate_range_kernel_t kernel)
{
    sgl_neon_rotate_data_t result;

    result.context = context;
    result.src = src;
    result.dst = dst;
    result.lut = lut;
    result.kernel = kernel;

    return result;
}

static void sgl_neon_rotate_copy_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_size_t offset;
    sgl_size_t end;
    uint8x16_t value;

    (void)context;
    (void)lut;
    offset = (sgl_size_t)(sgl_uint32_t)start;
    end = offset + (sgl_size_t)(sgl_uint32_t)count;
    while ((end - offset) >= SGL_NEON_ROTATE_VECTOR_BYTES) {
        value = vld1q_u8(&src[offset]);
        vst1q_u8(&dst[offset], value);
        offset += SGL_NEON_ROTATE_VECTOR_BYTES;
    }
    while (offset < end) {
        dst[offset] = src[offset];
        ++offset;
    }
}

static uint8x16_t sgl_neon_reverse_u8(uint8x16_t value)
{
    uint8x16_t reversed;
    uint8x16_t result;

    /* Reverse bytes within each 64-bit half, then exchange the halves. */
    reversed = vrev64q_u8(value);
    result = vextq_u8(reversed, reversed, 8);

    return result;
}

static uint16x8_t sgl_neon_reverse_u16(uint16x8_t value)
{
    uint16x8_t reversed;
    uint16x8_t result;

    /* Reverse 16-bit pixels within each half, then exchange the halves. */
    reversed = vrev64q_u16(value);
    result = vextq_u16(reversed, reversed, 4);

    return result;
}

static uint32x4_t sgl_neon_reverse_u32(uint32x4_t value)
{
    uint32x4_t reversed;
    uint32x4_t result;

    /* Reverse 32-bit pixels within each half, then exchange the halves. */
    reversed = vrev64q_u32(value);
    result = vextq_u32(reversed, reversed, 2);

    return result;
}

static void sgl_neon_rotate_180_u8_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_size_t pixels;
    sgl_size_t offset;
    sgl_size_t end;
    uint8x16_t value;

    (void)lut;
    pixels = (sgl_size_t)(sgl_uint32_t)context->src_width *
        (sgl_size_t)(sgl_uint32_t)context->src_height;
    offset = (sgl_size_t)(sgl_uint32_t)start;
    end = offset + (sgl_size_t)(sgl_uint32_t)count;
    while ((end - offset) >= 16U) {
        value = vld1q_u8(&src[pixels - offset - 16U]);
        value = sgl_neon_reverse_u8(value);
        vst1q_u8(&dst[offset], value);
        offset += 16U;
    }
    while (offset < end) {
        dst[offset] = src[pixels - offset - 1U];
        ++offset;
    }
}

static void sgl_neon_rotate_180_u16_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_size_t pixels;
    sgl_size_t dst_pixel;
    sgl_size_t src_pixel;
    sgl_size_t end;
    uint8x16_t bytes;
    uint16x8_t value;

    (void)lut;
    pixels = (sgl_size_t)(sgl_uint32_t)context->src_width *
        (sgl_size_t)(sgl_uint32_t)context->src_height;
    dst_pixel = (sgl_size_t)(sgl_uint32_t)start;
    end = dst_pixel + (sgl_size_t)(sgl_uint32_t)count;
    while ((end - dst_pixel) >= 8U) {
        src_pixel = pixels - dst_pixel - 8U;
        bytes = vld1q_u8(&src[src_pixel << 1U]);
        value = vreinterpretq_u16_u8(bytes);
        value = sgl_neon_reverse_u16(value);
        vst1q_u8(&dst[dst_pixel << 1U], vreinterpretq_u8_u16(value));
        dst_pixel += 8U;
    }
    while (dst_pixel < end) {
        src_pixel = pixels - dst_pixel - 1U;
        dst[dst_pixel << 1U] = src[src_pixel << 1U];
        dst[(dst_pixel << 1U) + 1U] = src[(src_pixel << 1U) + 1U];
        ++dst_pixel;
    }
}

static void sgl_neon_rotate_180_u32_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_size_t pixels;
    sgl_size_t dst_pixel;
    sgl_size_t src_pixel;
    sgl_size_t end;
    uint8x16_t bytes;
    uint32x4_t value;

    (void)lut;
    pixels = (sgl_size_t)(sgl_uint32_t)context->src_width *
        (sgl_size_t)(sgl_uint32_t)context->src_height;
    dst_pixel = (sgl_size_t)(sgl_uint32_t)start;
    end = dst_pixel + (sgl_size_t)(sgl_uint32_t)count;
    while ((end - dst_pixel) >= 4U) {
        src_pixel = pixels - dst_pixel - 4U;
        bytes = vld1q_u8(&src[src_pixel << 2U]);
        value = vreinterpretq_u32_u8(bytes);
        value = sgl_neon_reverse_u32(value);
        vst1q_u8(&dst[dst_pixel << 2U], vreinterpretq_u8_u32(value));
        dst_pixel += 4U;
    }
    while (dst_pixel < end) {
        src_pixel = pixels - dst_pixel - 1U;
        dst[dst_pixel << 2U] = src[src_pixel << 2U];
        dst[(dst_pixel << 2U) + 1U] = src[(src_pixel << 2U) + 1U];
        dst[(dst_pixel << 2U) + 2U] = src[(src_pixel << 2U) + 2U];
        dst[(dst_pixel << 2U) + 3U] = src[(src_pixel << 2U) + 3U];
        ++dst_pixel;
    }
}

static void sgl_neon_rotate_copy_rgba_pixel(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_size_t src_pixel,
    sgl_size_t dst_pixel)
{
    sgl_size_t src_offset;
    sgl_size_t dst_offset;

    /* A two-bit shift converts a 4-byte RGBA pixel index to a byte offset. */
    src_offset = src_pixel << 2U;
    dst_offset = dst_pixel << 2U;
    dst[dst_offset] = src[src_offset];
    dst[dst_offset + 1U] = src[src_offset + 1U];
    dst[dst_offset + 2U] = src[src_offset + 2U];
    dst[dst_offset + 3U] = src[src_offset + 3U];
}

static void sgl_neon_rotate_clear_rgba_pixel(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_size_t dst_pixel)
{
    sgl_size_t dst_offset;

    dst_offset = dst_pixel << 2U;
    dst[dst_offset] = 0U;
    dst[dst_offset + 1U] = 0U;
    dst[dst_offset + 2U] = 0U;
    dst[dst_offset + 3U] = 0U;
}

static sgl_neon_rotate_rgba_tile_t sgl_neon_rotate_load_rgba_tile(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t x,
    sgl_int32_t y)
{
    uint32x4x2_t rows01;
    uint32x4x2_t rows23;
    uint32x4_t row0;
    uint32x4_t row1;
    uint32x4_t row2;
    uint32x4_t row3;
    sgl_size_t pixel;
    sgl_size_t stride;
    sgl_neon_rotate_rgba_tile_t result;

    pixel = ((sgl_size_t)(sgl_uint32_t)y *
             (sgl_size_t)(sgl_uint32_t)context->src_width) +
        (sgl_size_t)(sgl_uint32_t)x;
    stride = (sgl_size_t)(sgl_uint32_t)context->src_width << 2U;
    row0 = vreinterpretq_u32_u8(vld1q_u8(&src[pixel << 2U]));
    row1 = vreinterpretq_u32_u8(vld1q_u8(&src[(pixel << 2U) + stride]));
    row2 = vreinterpretq_u32_u8(vld1q_u8(&src[(pixel << 2U) +
                                              (stride << 1U)]));
    row3 = vreinterpretq_u32_u8(vld1q_u8(&src[(pixel << 2U) +
                                              (stride * 3U)]));

    /* Two 32-bit transposes form four source columns from four source rows. */
    rows01 = vtrnq_u32(row0, row1);
    rows23 = vtrnq_u32(row2, row3);
    result.column0 = vcombine_u32(
        vget_low_u32(rows01.val[0]), vget_low_u32(rows23.val[0]));
    result.column1 = vcombine_u32(
        vget_low_u32(rows01.val[1]), vget_low_u32(rows23.val[1]));
    result.column2 = vcombine_u32(
        vget_high_u32(rows01.val[0]), vget_high_u32(rows23.val[0]));
    result.column3 = vcombine_u32(
        vget_high_u32(rows01.val[1]), vget_high_u32(rows23.val[1]));

    return result;
}

static void sgl_neon_rotate_store_rgba_vector(
    const sgl_rotate_context_t *context,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t row,
    sgl_int32_t column,
    uint32x4_t value)
{
    sgl_size_t pixel;

    pixel = ((sgl_size_t)(sgl_uint32_t)row *
             (sgl_size_t)(sgl_uint32_t)context->dst_width) +
        (sgl_size_t)(sgl_uint32_t)column;
    vst1q_u8(&dst[pixel << 2U], vreinterpretq_u8_u32(value));
}

static void sgl_neon_rotate_90_rgba_column(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t source_x,
    sgl_int32_t source_y,
    sgl_int32_t count)
{
    sgl_int32_t source_end;

    source_end = source_y + count;
    while (source_y < source_end) {
        sgl_size_t src_pixel;
        sgl_size_t dst_pixel;

        src_pixel = ((sgl_size_t)(sgl_uint32_t)source_y *
                     (sgl_size_t)(sgl_uint32_t)context->src_width) +
            (sgl_size_t)(sgl_uint32_t)source_x;
        dst_pixel = ((sgl_size_t)(sgl_uint32_t)source_x *
                     (sgl_size_t)(sgl_uint32_t)context->dst_width) +
            (sgl_size_t)(sgl_uint32_t)(context->src_height - source_y - 1);
        sgl_neon_rotate_copy_rgba_pixel(src, dst, src_pixel, dst_pixel);
        ++source_y;
    }
}

static void sgl_neon_rotate_270_rgba_column(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t source_x,
    sgl_int32_t source_y,
    sgl_int32_t count)
{
    sgl_int32_t source_end;

    source_end = source_y + count;
    while (source_y < source_end) {
        sgl_size_t src_pixel;
        sgl_size_t dst_pixel;

        src_pixel = ((sgl_size_t)(sgl_uint32_t)source_y *
                     (sgl_size_t)(sgl_uint32_t)context->src_width) +
            (sgl_size_t)(sgl_uint32_t)source_x;
        dst_pixel = ((sgl_size_t)(sgl_uint32_t)(
                         context->src_width - source_x - 1) *
                     (sgl_size_t)(sgl_uint32_t)context->dst_width) +
            (sgl_size_t)(sgl_uint32_t)source_y;
        sgl_neon_rotate_copy_rgba_pixel(src, dst, src_pixel, dst_pixel);
        ++source_y;
    }
}

static void sgl_neon_rotate_90_rgba_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t x;
    sgl_int32_t end;

    (void)lut;
    x = start;
    end = start + count;
    while ((end - x) >= SGL_NEON_ROTATE_RGBA_TILE) {
        sgl_int32_t y = 0;

        while ((context->src_height - y) >= SGL_NEON_ROTATE_RGBA_TILE) {
            sgl_int32_t dst_column;
            sgl_neon_rotate_rgba_tile_t tile;

            tile = sgl_neon_rotate_load_rgba_tile(context, src, x, y);
            dst_column = context->src_height - y -
                SGL_NEON_ROTATE_RGBA_TILE;
            sgl_neon_rotate_store_rgba_vector(
                context,
                dst,
                x,
                dst_column,
                sgl_neon_reverse_u32(tile.column0));
            sgl_neon_rotate_store_rgba_vector(
                context,
                dst,
                x + 1,
                dst_column,
                sgl_neon_reverse_u32(tile.column1));
            sgl_neon_rotate_store_rgba_vector(
                context,
                dst,
                x + 2,
                dst_column,
                sgl_neon_reverse_u32(tile.column2));
            sgl_neon_rotate_store_rgba_vector(
                context,
                dst,
                x + 3,
                dst_column,
                sgl_neon_reverse_u32(tile.column3));
            y += SGL_NEON_ROTATE_RGBA_TILE;
        }
        if (y < context->src_height) {
            sgl_neon_rotate_90_rgba_column(
                context, src, dst, x, y, context->src_height - y);
            sgl_neon_rotate_90_rgba_column(
                context, src, dst, x + 1, y, context->src_height - y);
            sgl_neon_rotate_90_rgba_column(
                context, src, dst, x + 2, y, context->src_height - y);
            sgl_neon_rotate_90_rgba_column(
                context, src, dst, x + 3, y, context->src_height - y);
        }
        x += SGL_NEON_ROTATE_RGBA_TILE;
    }
    while (x < end) {
        sgl_neon_rotate_90_rgba_column(
            context, src, dst, x, 0, context->src_height);
        ++x;
    }
}

static void sgl_neon_rotate_270_rgba_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t x;
    sgl_int32_t end;

    (void)lut;
    x = start;
    end = start + count;
    while ((end - x) >= SGL_NEON_ROTATE_RGBA_TILE) {
        sgl_int32_t y = 0;

        while ((context->src_height - y) >= SGL_NEON_ROTATE_RGBA_TILE) {
            sgl_int32_t dst_row;
            sgl_neon_rotate_rgba_tile_t tile;

            tile = sgl_neon_rotate_load_rgba_tile(context, src, x, y);
            dst_row = context->src_width - x - 1;
            sgl_neon_rotate_store_rgba_vector(
                context, dst, dst_row, y, tile.column0);
            sgl_neon_rotate_store_rgba_vector(
                context, dst, dst_row - 1, y, tile.column1);
            sgl_neon_rotate_store_rgba_vector(
                context, dst, dst_row - 2, y, tile.column2);
            sgl_neon_rotate_store_rgba_vector(
                context, dst, dst_row - 3, y, tile.column3);
            y += SGL_NEON_ROTATE_RGBA_TILE;
        }
        if (y < context->src_height) {
            sgl_neon_rotate_270_rgba_column(
                context, src, dst, x, y, context->src_height - y);
            sgl_neon_rotate_270_rgba_column(
                context, src, dst, x + 1, y, context->src_height - y);
            sgl_neon_rotate_270_rgba_column(
                context, src, dst, x + 2, y, context->src_height - y);
            sgl_neon_rotate_270_rgba_column(
                context, src, dst, x + 3, y, context->src_height - y);
        }
        x += SGL_NEON_ROTATE_RGBA_TILE;
    }
    while (x < end) {
        sgl_neon_rotate_270_rgba_column(
            context, src, dst, x, 0, context->src_height);
        ++x;
    }
}

static const uint32_t *sgl_neon_rotate_rgba_pixel_from_bytes(
    const sgl_uint8_t *pixel)
{
    const uint32_t *result;

    /* NEON lane loads accept unaligned RGBA addresses on supported targets. */
    /* cppcheck-suppress misra-c2012-11.3 */
    result = (const uint32_t *)pixel;

    return result;
}

static uint32x4_t sgl_neon_rotate_gather_valid_rgba(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t source0,
    sgl_int32_t source1,
    sgl_int32_t source2,
    sgl_int32_t source3)
{
    uint32x4_t result;

    result = vld1q_dup_u32(
        sgl_neon_rotate_rgba_pixel_from_bytes(
            &src[(sgl_size_t)(sgl_uint32_t)source0 << 2U]));
    result = vld1q_lane_u32(
        sgl_neon_rotate_rgba_pixel_from_bytes(
            &src[(sgl_size_t)(sgl_uint32_t)source1 << 2U]),
        result,
        1);
    result = vld1q_lane_u32(
        sgl_neon_rotate_rgba_pixel_from_bytes(
            &src[(sgl_size_t)(sgl_uint32_t)source2 << 2U]),
        result,
        2);
    result = vld1q_lane_u32(
        sgl_neon_rotate_rgba_pixel_from_bytes(
            &src[(sgl_size_t)(sgl_uint32_t)source3 << 2U]),
        result,
        3);

    return result;
}

static void sgl_neon_rotate_clear_rgba_range(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_size_t start,
    sgl_int32_t count)
{
    uint32x4_t zeros;
    sgl_size_t pixel;
    sgl_size_t end;

    zeros = vdupq_n_u32(0U);
    pixel = start;
    end = start + (sgl_size_t)(sgl_uint32_t)count;
    while ((end - pixel) >= 4U) {
        vst1q_u8(
            &dst[pixel << 2U],
            vreinterpretq_u8_u32(zeros));
        pixel += 4U;
    }
    while (pixel < end) {
        sgl_neon_rotate_clear_rgba_pixel(dst, pixel);
        ++pixel;
    }
}

static void sgl_neon_rotate_lookup_rgba_row(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t dst_y)
{
    sgl_int32_t dst_x;
    sgl_int32_t valid_start;
    sgl_int32_t valid_end;
    sgl_size_t row_pixel;
    sgl_size_t dst_pixel;

    row_pixel = (sgl_size_t)(sgl_uint32_t)dst_y *
        (sgl_size_t)(sgl_uint32_t)context->dst_width;
    valid_start = lut->row_ranges[
        (sgl_size_t)(sgl_uint32_t)dst_y << 1U];
    valid_end = lut->row_ranges[
        ((sgl_size_t)(sgl_uint32_t)dst_y << 1U) + 1U];
    sgl_neon_rotate_clear_rgba_range(
        dst, row_pixel, valid_start);
    dst_x = valid_start;
    dst_pixel = row_pixel + (sgl_size_t)(sgl_uint32_t)valid_start;
    while ((valid_end - dst_x) >= 4) {
        uint32x4_t pixels;

        pixels = sgl_neon_rotate_gather_valid_rgba(
            src,
            lut->source_offsets[dst_pixel],
            lut->source_offsets[dst_pixel + 1U],
            lut->source_offsets[dst_pixel + 2U],
            lut->source_offsets[dst_pixel + 3U]);
        vst1q_u8(
            &dst[dst_pixel << 2U],
            vreinterpretq_u8_u32(pixels));
        dst_x += 4;
        dst_pixel += 4U;
    }
    while (dst_x < valid_end) {
        sgl_neon_rotate_copy_rgba_pixel(
            src,
            dst,
            (sgl_size_t)(sgl_uint32_t)lut->source_offsets[dst_pixel],
            dst_pixel);
        ++dst_x;
        ++dst_pixel;
    }
    sgl_neon_rotate_clear_rgba_range(
        dst,
        row_pixel + (sgl_size_t)(sgl_uint32_t)valid_end,
        context->dst_width - valid_end);
}

static void sgl_neon_rotate_lookup_rgba_range(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t end;

    row = start;
    end = start + count;
    while (row < end) {
        sgl_neon_rotate_lookup_rgba_row(
            context, src, dst, lut, row);
        ++row;
    }
}

static sgl_result_t sgl_neon_rotate_run_identity(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst)
{
    sgl_neon_rotate_data_t data;
    sgl_result_t result;

    (void)pool;
    data = sgl_neon_rotate_make_data(
        context, src, dst, SGL_NULL, sgl_neon_rotate_copy_range);
    /* A linear NEON copy already saturates this small transfer efficiently. */
    result = sgl_neon_rotate_execute(
        SGL_NULL,
        &data,
        (sgl_int32_t)context->dst_bytes,
        (sgl_int32_t)SGL_NEON_ROTATE_VECTOR_BYTES,
        SGL_NEON_ROTATE_SCHEDULE_STATIC);

    return result;
}

static sgl_result_t sgl_neon_rotate_run_180(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_bool_t *handled)
{
    sgl_neon_rotate_data_t data;
    sgl_int32_t alignment = 1;
    sgl_size_t pixels;
    sgl_result_t result = SGL_SUCCESS;

    data = sgl_neon_rotate_make_data(
        context, src, dst, SGL_NULL, sgl_neon_rotate_180_u8_range);
    switch (context->bpp) {
    case SGL_BPP8:
        data.kernel = sgl_neon_rotate_180_u8_range;
        alignment = 16;
        break;
    case SGL_BPP16:
        data.kernel = sgl_neon_rotate_180_u16_range;
        alignment = 8;
        break;
    case SGL_BPP32:
        data.kernel = sgl_neon_rotate_180_u32_range;
        alignment = 4;
        break;
    default:
        *handled = SGL_FALSE;
        break;
    }
    if (*handled == SGL_TRUE) {
        pixels = (sgl_size_t)(sgl_uint32_t)context->src_width *
            (sgl_size_t)(sgl_uint32_t)context->src_height;
        /* Reverse-copy is bandwidth-bound; thread dispatch does not amortize. */
        result = sgl_neon_rotate_execute(
            SGL_NULL,
            &data,
            (sgl_int32_t)pixels,
            alignment,
            SGL_NEON_ROTATE_SCHEDULE_STATIC);
    }

    (void)pool;

    return result;
}

static sgl_result_t sgl_neon_rotate_run_right_angle(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst)
{
    sgl_neon_rotate_data_t data;
    sgl_result_t result;

    data = sgl_neon_rotate_make_data(
        context,
        src,
        dst,
        SGL_NULL,
        sgl_neon_rotate_90_rgba_range);
    switch (context->normalized_angle) {
    case SGL_NEON_ROTATE_90_Q16:
        data.kernel = sgl_neon_rotate_90_rgba_range;
        break;
    default:
        data.kernel = sgl_neon_rotate_270_rgba_range;
        break;
    }
    result = sgl_neon_rotate_execute(
        pool,
        &data,
        context->src_width,
        SGL_NEON_ROTATE_RGBA_TILE,
        SGL_NEON_ROTATE_SCHEDULE_STATIC);

    return result;
}

static sgl_result_t sgl_neon_rotate_run_lookup_rgba(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut)
{
    sgl_neon_rotate_data_t data;
    sgl_result_t result;

    data = sgl_neon_rotate_make_data(
        context,
        src,
        dst,
        lut,
        sgl_neon_rotate_lookup_rgba_range);
    /* Valid row spans vary by angle; small dynamic chunks balance gathers. */
    result = sgl_neon_rotate_execute(
        pool,
        &data,
        context->dst_height,
        1,
        SGL_NEON_ROTATE_SCHEDULE_DYNAMIC);

    return result;
}

static sgl_result_t sgl_neon_rotate_special(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_bool_t *handled)
{
    sgl_result_t result = SGL_SUCCESS;

    *handled = SGL_TRUE;
    switch (context->normalized_angle) {
    case 0:
        result = sgl_neon_rotate_run_identity(pool, context, src, dst);
        break;
    case SGL_NEON_ROTATE_90_Q16:
    case SGL_NEON_ROTATE_270_Q16:
        if (context->bpp == SGL_BPP32) {
            result = sgl_neon_rotate_run_right_angle(
                pool, context, src, dst);
        }
        else {
            *handled = SGL_FALSE;
        }
        break;
    case SGL_NEON_ROTATE_180_Q16:
        result = sgl_neon_rotate_run_180(
            pool, context, src, dst, handled);
        break;
    default:
        *handled = SGL_FALSE;
        break;
    }

    return result;
}

sgl_result_t sgl_simd_rotate(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_rotate_lookup_t *SGL_RESTRICT ext_lut,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp,
    sgl_float64_t angle)
{
    sgl_rotate_context_t context;
    sgl_result_t result;
    sgl_bool_t handled = SGL_FALSE;

    result = sgl_rotate_validate_buffers(src, dst);
    if (result == SGL_SUCCESS) {
        result = sgl_rotate_context_initialize(
            src_width,
            src_height,
            bpp,
            angle,
            &context);
    }
    if (result == SGL_SUCCESS) {
        result = sgl_rotate_validate_lut(&context, ext_lut);
    }
    if (result == SGL_SUCCESS) {
        result = sgl_neon_rotate_special(
            pool, &context, src, dst, &handled);
    }
    if ((result == SGL_SUCCESS) && (handled != SGL_TRUE)) {
        /*
         * NEON has no arbitrary-address gather instruction. Precomputed LUT
         * offsets make four lane loads profitable, while calculating four
         * coordinates before the same lane loads is slower than scalar code.
         */
        if ((context.bpp == SGL_BPP32) && (ext_lut != SGL_NULL)) {
            result = sgl_neon_rotate_run_lookup_rgba(
                pool, &context, src, dst, ext_lut);
        }
        else {
            result = sgl_rotate_run_scalar(
                pool, &context, src, dst, ext_lut);
        }
    }

    return result;
}
