/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "nearest_neighbor.h"
#include "nearest_neighbor_packed.h"
#include "resize_bitops.h"

/*
 * Keep the fixed-format downscale loops out of the larger NEON translation
 * unit so its upscale instruction path remains compact.  Passing source and
 * destination as separate restrict-qualified arguments also lets the compiler
 * combine byte copies into 16/32-bit unaligned-safe memory operations.
 */
static SGL_NOINLINE void sgl_resize_nearest_neighbor_copy_range_bpp16(
    sgl_int32_t start_row,
    sgl_int32_t row_count,
    const sgl_nearest_neighbor_lookup_t *SGL_RESTRICT lut,
    const sgl_uint8_t *SGL_RESTRICT source,
    sgl_int32_t src_stride,
    sgl_uint8_t *SGL_RESTRICT destination,
    sgl_int32_t dst_stride)
{
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t col;
    sgl_int32_t end_row;
    sgl_int32_t row;

    x = lut->x;
    end_row = start_row + row_count;
    if (end_row > lut->d_height) {
        end_row = lut->d_height;
    }

    for (row = start_row; row < end_row; ++row) {
        src_y_buf = &source[lut->y[row] * src_stride];
        dst = &destination[row * dst_stride];
        for (col = 0; col < lut->d_width; ++col) {
            src = &src_y_buf[SGL_RESIZE_BPP16_BYTE_OFFSET(x[col])];
            dst[1] = src[1];
            dst[0] = src[0];
            dst = &dst[SGL_BPP16];
        }
    }
}

static SGL_NOINLINE void sgl_resize_nearest_neighbor_copy_range_bpp24(
    sgl_int32_t start_row,
    sgl_int32_t row_count,
    const sgl_nearest_neighbor_lookup_t *SGL_RESTRICT lut,
    const sgl_uint8_t *SGL_RESTRICT source,
    sgl_int32_t src_stride,
    sgl_uint8_t *SGL_RESTRICT destination,
    sgl_int32_t dst_stride)
{
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t col;
    sgl_int32_t end_row;
    sgl_int32_t row;

    x = lut->x;
    end_row = start_row + row_count;
    if (end_row > lut->d_height) {
        end_row = lut->d_height;
    }

    for (row = start_row; row < end_row; ++row) {
        src_y_buf = &source[lut->y[row] * src_stride];
        dst = &destination[row * dst_stride];
        for (col = 0; col < lut->d_width; ++col) {
            src = &src_y_buf[x[col] * SGL_BPP24];
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            dst = &dst[SGL_BPP24];
        }
    }
}

static SGL_NOINLINE void sgl_resize_nearest_neighbor_copy_range_bpp32(
    sgl_int32_t start_row,
    sgl_int32_t row_count,
    const sgl_nearest_neighbor_lookup_t *SGL_RESTRICT lut,
    const sgl_uint8_t *SGL_RESTRICT source,
    sgl_int32_t src_stride,
    sgl_uint8_t *SGL_RESTRICT destination,
    sgl_int32_t dst_stride)
{
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t col;
    sgl_int32_t end_row;
    sgl_int32_t row;

    x = lut->x;
    end_row = start_row + row_count;
    if (end_row > lut->d_height) {
        end_row = lut->d_height;
    }

    for (row = start_row; row < end_row; ++row) {
        src_y_buf = &source[lut->y[row] * src_stride];
        dst = &destination[row * dst_stride];
        for (col = 0; col < lut->d_width; ++col) {
            src = &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col])];
            dst[3] = src[3];
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            dst = &dst[SGL_BPP32];
        }
    }
}

void sgl_resize_nearest_neighbor_dispatch_packed_range(
    sgl_int32_t start_row,
    sgl_int32_t row_count,
    sgl_nearest_neighbor_data_t *data)
{
    switch (data->bpp) {
    case SGL_BPP32:
        sgl_resize_nearest_neighbor_copy_range_bpp32(
            start_row, row_count, data->lut,
            data->src, data->src_stride,
            data->dst, data->dst_stride);
        break;
    case SGL_BPP24:
        sgl_resize_nearest_neighbor_copy_range_bpp24(
            start_row, row_count, data->lut,
            data->src, data->src_stride,
            data->dst, data->dst_stride);
        break;
    case SGL_BPP16:
        sgl_resize_nearest_neighbor_copy_range_bpp16(
            start_row, row_count, data->lut,
            data->src, data->src_stride,
            data->dst, data->dst_stride);
        break;
    default:
        sgl_resize_nearest_neighbor_copy_packed_range(
            start_row, row_count, data);
        break;
    }
}

sgl_nearest_neighbor_lookup_t *sgl_generic_create_nearest_neighbor_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_nearest_neighbor_lookup_t *lut;
    sgl_int32_t col;
    sgl_int32_t row;
    sgl_int32_t rx;
    sgl_int32_t ry;

    lut = sgl_memory_as_nearest_neighbor_lookup(
        sgl_malloc(sizeof(sgl_nearest_neighbor_lookup_t)));
    if (lut != SGL_NULL) {
        lut->d_width = d_width;
        lut->d_height = d_height;
        lut->s_width = s_width;
        lut->s_height = s_height;
        lut->x = sgl_memory_as_int32(sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width));
        lut->y = sgl_memory_as_int32(sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height));

        if ((lut->x != SGL_NULL) && (lut->y != SGL_NULL)) {
            /* create 'column' lookup table */
            for (col = 0; col < d_width; ++col) {
                rx = SGL_DIV_ROUNDUP(col * (s_width - 1), d_width - 1);
                lut->x[col] = (rx >= s_width) ? (s_width - 1) : rx;
            }

            /* create 'row' lookup table */
            for (row = 0; row < d_height; ++row) {
                ry = SGL_DIV_ROUNDUP(row * (s_height - 1), d_height - 1);
                lut->y[row] = (ry >= s_height) ? (s_height - 1) : ry;
            }
        }
        else {
            SGL_SAFE_FREE(lut->x);
            SGL_SAFE_FREE(lut->y);
            SGL_SAFE_FREE(lut);
            lut = SGL_NULL;
        }
    }
    else {
        lut = SGL_NULL;
    }

    return lut;
}

void sgl_generic_destroy_nearest_neighbor_lut(sgl_nearest_neighbor_lookup_t *lut)
{
    if (lut != SGL_NULL) {
        SGL_SAFE_FREE(lut->x);
        SGL_SAFE_FREE(lut->y);
        sgl_free(lut);
    }
}
