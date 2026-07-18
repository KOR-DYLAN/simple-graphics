/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_NEAREST_NEIGHBOR_PACKED_H_
#define SGL_NEAREST_NEIGHBOR_PACKED_H_

#include "resize_bitops.h"

static SGL_ALWAYS_INLINE void sgl_resize_nearest_neighbor_copy_packed_range(
    sgl_int32_t start_row,
    sgl_int32_t row_count,
    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut;
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t d_width;
    sgl_int32_t end_row;
    sgl_int32_t row;
    sgl_int32_t bpp;

    lut = data->lut;
    d_width = lut->d_width;
    bpp = data->bpp;
    x = lut->x;
    end_row = start_row + row_count;
    if (end_row > lut->d_height) {
        end_row = lut->d_height;
    }

    switch (bpp) {
    case SGL_BPP32:
        for (row = start_row; row < end_row; ++row) {
            src_y_buf = &data->src[lut->y[row] * data->src_stride];
            dst = &data->dst[row * data->dst_stride];
            for (col = 0; col < d_width; ++col) {
                src = &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col])];
                dst[3] = src[3];
                dst[2] = src[2];
                dst[1] = src[1];
                dst[0] = src[0];
                dst = &dst[SGL_BPP32];
            }
        }
        break;
    case SGL_BPP24:
        for (row = start_row; row < end_row; ++row) {
            src_y_buf = &data->src[lut->y[row] * data->src_stride];
            dst = &data->dst[row * data->dst_stride];
            for (col = 0; col < d_width; ++col) {
                src = &src_y_buf[x[col] * SGL_BPP24];
                dst[2] = src[2];
                dst[1] = src[1];
                dst[0] = src[0];
                dst = &dst[SGL_BPP24];
            }
        }
        break;
    case SGL_BPP16:
        for (row = start_row; row < end_row; ++row) {
            src_y_buf = &data->src[lut->y[row] * data->src_stride];
            dst = &data->dst[row * data->dst_stride];
            for (col = 0; col < d_width; ++col) {
                src = &src_y_buf[SGL_RESIZE_BPP16_BYTE_OFFSET(x[col])];
                dst[1] = src[1];
                dst[0] = src[0];
                dst = &dst[SGL_BPP16];
            }
        }
        break;
    case SGL_BPP8:
        for (row = start_row; row < end_row; ++row) {
            src_y_buf = &data->src[lut->y[row] * data->src_stride];
            dst = &data->dst[row * data->dst_stride];
            for (col = 0; col < d_width; ++col) {
                src = &src_y_buf[x[col]];
                dst[0] = src[0];
                dst = &dst[SGL_BPP8];
            }
        }
        break;
    default:
        for (row = start_row; row < end_row; ++row) {
            src_y_buf = &data->src[lut->y[row] * data->src_stride];
            dst = &data->dst[row * data->dst_stride];
            for (col = 0; col < d_width; ++col) {
                src = &src_y_buf[x[col] * bpp];
                for (ch = 0; ch < bpp; ++ch) {
                    dst[ch] = src[ch];
                }
                dst = &dst[bpp];
            }
        }
        break;
    }
}

#endif  /* SGL_NEAREST_NEIGHBOR_PACKED_H_ */
