/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "neon_common.h"

static sgl_bool_t sgl_neon_is_yuv(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    if ((sgl_neon_is_yuv444(format) == SGL_TRUE) ||
        (sgl_neon_is_yuv422(format) == SGL_TRUE) ||
        (sgl_neon_is_yuv420(format) == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

static void sgl_neon_run_yuv444_to_yuv444(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_size_t pixels,
    sgl_size_t start,
    sgl_size_t count)
{
    sgl_size_t pixel;
    sgl_size_t end;
    sgl_neon_yuv_t value;

    pixel = start;
    end = start + count;
    while ((end - pixel) >= SGL_NEON_LANES) {
        value = sgl_neon_load_yuv444(src, src_format, pixels, pixel);
        sgl_neon_store_yuv444(dst, dst_format, pixels, pixel, value);
        pixel += SGL_NEON_LANES;
    }
    if (pixel < end) {
        /* Reuse the final complete vector for a byte-exact YUV444 tail. */
        pixel = end - SGL_NEON_LANES;
        value = sgl_neon_load_yuv444(src, src_format, pixels, pixel);
        sgl_neon_store_yuv444(dst, dst_format, pixels, pixel, value);
    }
}

static void sgl_neon_yuv_run_to_444(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t x;
    sgl_int32_t end;
    sgl_size_t pixel;
    sgl_neon_yuv_t yuv;

    end = start + count;
    for (row = start; row < end; ++row) {
        for (x = 0; x < data->width; x += (sgl_int32_t)SGL_NEON_LANES) {
            pixel = ((sgl_size_t)(sgl_uint32_t)row *
                     (sgl_size_t)(sgl_uint32_t)data->width) +
                (sgl_size_t)(sgl_uint32_t)x;
            yuv = sgl_neon_yuv_load(data, row, x);
            sgl_neon_store_yuv444(
                data->dst, data->dst_format, data->pixels, pixel, yuv);
        }
    }
}

static void sgl_neon_yuv_run_to_422(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t x;
    sgl_int32_t end;
    sgl_neon_yuv_t yuv;

    end = start + count;
    for (row = start; row < end; ++row) {
        for (x = 0; x < data->width; x += (sgl_int32_t)SGL_NEON_LANES) {
            yuv = sgl_neon_yuv_load(data, row, x);
            sgl_neon_yuv_store_422(data, row, x, yuv);
        }
    }
}

static void sgl_neon_yuv_run_to_420(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t x;
    sgl_int32_t end;
    sgl_neon_yuv_t top;
    sgl_neon_yuv_t bottom;

    end = start + count;
    for (row = start; row < end; row += 2) {
        for (x = 0; x < data->width; x += (sgl_int32_t)SGL_NEON_LANES) {
            top = sgl_neon_yuv_load(data, row, x);
            bottom = sgl_neon_yuv_load(data, row + 1, x);
            sgl_neon_yuv_store_420(data, row, x, top, bottom);
        }
    }
}

static sgl_result_t sgl_neon_yuv_to_yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_neon_yuv_kernel_t kernel = sgl_neon_yuv_run_to_444;
    sgl_int32_t alignment = 1;
    sgl_result_t result = SGL_ERROR_NOT_SUPPORTED;

    if (sgl_neon_is_yuv422(dst_format) == SGL_TRUE) {
        kernel = sgl_neon_yuv_run_to_422;
    }
    if (sgl_neon_is_yuv420(dst_format) == SGL_TRUE) {
        kernel = sgl_neon_yuv_run_to_420;
        alignment = 2;
    }
    if ((sgl_neon_is_yuv(src_format) == SGL_TRUE) &&
        (sgl_neon_is_yuv(dst_format) == SGL_TRUE)) {
        /* 4:2:0 destinations keep each two-row chroma group in one range. */
        result = sgl_neon_yuv_run(
            pool,
            src,
            src_format,
            dst,
            dst_format,
            width,
            height,
            alignment,
            kernel);
    }

    return result;
}

sgl_result_t sgl_simd_yuv2yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_result_t result;
    sgl_size_t pixels;
    sgl_neon_convert_data_t data;

    result = sgl_neon_validate(src, dst, width, height);
    if ((result == SGL_SUCCESS) && (src_format == dst_format)) {
        /*
         * Equal FourCC values describe the same packed or planar byte layout.
         * The generic identity path performs the checked SIMD-aware memcpy and
         * avoids loading and storing every component through conversion lanes.
         */
        result = sgl_generic_yuv2yuv(
            pool,
            src,
            src_format,
            dst,
            dst_format,
            width,
            height);
    }
    if ((result == SGL_SUCCESS) && (src_format != dst_format)) {
        result = sgl_neon_get_pixel_count(width, height, &pixels);
    }
    if ((result == SGL_SUCCESS) && (src_format != dst_format)) {
        if ((pixels >= SGL_NEON_LANES) &&
            (sgl_neon_is_yuv444(src_format) == SGL_TRUE) &&
            (sgl_neon_is_yuv444(dst_format) == SGL_TRUE)) {
            data = sgl_neon_convert_make_data(
                src,
                src_format,
                dst,
                dst_format,
                pixels,
                sgl_neon_run_yuv444_to_yuv444);
            result = sgl_neon_convert_execute(pool, &data);
        }
        else {
            result = sgl_neon_yuv_to_yuv(
                pool,
                src,
                src_format,
                dst,
                dst_format,
                width,
                height);
            if (result == SGL_ERROR_NOT_SUPPORTED) {
                /* Generic conversion covers odd dimensions and short rows. */
                result = sgl_generic_yuv2yuv(
                    pool,
                    src,
                    src_format,
                    dst,
                    dst_format,
                    width,
                    height);
            }
        }
    }

    return result;
}
