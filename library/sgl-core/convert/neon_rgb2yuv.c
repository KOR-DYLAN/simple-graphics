/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "neon_common.h"

#define SGL_NEON_CONVERT_CHROMA_OFFSET    (128)

static int16x8_t sgl_neon_divide_256_s16(int16x8_t value)
{
    int16x8_t negative;
    int16x8_t correction;
    int16x8_t adjusted;
    int16x8_t result;

    negative = vshrq_n_s16(value, 15);
    correction = vandq_s16(negative, vdupq_n_s16(255));
    adjusted = vaddq_s16(value, correction);
    /*
     * Remove eight coefficient fractional bits. Adding 255 only to negative
     * lanes changes arithmetic-shift floor into C99 truncation toward zero.
     */
    result = vshrq_n_s16(adjusted, 8);

    return result;
}

static uint8x8_t sgl_neon_pack_chroma(int16x8_t value)
{
    int16x8_t adjusted;
    int8x8_t narrowed;
    uint8x8_t result;

    adjusted = sgl_neon_divide_256_s16(value);
    adjusted = vaddq_s16(
        adjusted, vdupq_n_s16(SGL_NEON_CONVERT_CHROMA_OFFSET));
    /*
     * The Q8 matrix bounds adjusted chroma to 0..255. Narrowing as signed and
     * reinterpreting preserves the exact low eight bits without saturation.
     */
    narrowed = vmovn_s16(adjusted);
    result = vreinterpret_u8_s8(narrowed);

    return result;
}

static uint8x8_t sgl_neon_rgb_to_luma(sgl_neon_rgb_t rgb)
{
    uint16x8_t y_acc;
    uint8x8_t result;

    y_acc = vmull_u8(rgb.r, vdup_n_u8(77U));
    y_acc = vmlal_u8(y_acc, rgb.g, vdup_n_u8(150U));
    y_acc = vmlal_u8(y_acc, rgb.b, vdup_n_u8(29U));
    /* Remove eight coefficient fractional bits: Q8 luma -> integer luma. */
    result = vshrn_n_u16(y_acc, 8);

    return result;
}

static uint8x8_t sgl_neon_rgb_to_chroma(
    sgl_neon_rgb_t rgb,
    sgl_int16_t red_coefficient,
    sgl_int16_t green_coefficient,
    sgl_int16_t blue_coefficient)
{
    int16x8_t red;
    int16x8_t green;
    int16x8_t blue;
    int16x8_t accumulator;
    uint8x8_t result;

    red = sgl_neon_widen_u8_to_s16(rgb.r);
    green = sgl_neon_widen_u8_to_s16(rgb.g);
    blue = sgl_neon_widen_u8_to_s16(rgb.b);
    accumulator = vmulq_n_s16(red, red_coefficient);
    accumulator = vmlaq_n_s16(
        accumulator, green, green_coefficient);
    accumulator = vmlaq_n_s16(
        accumulator, blue, blue_coefficient);
    result = sgl_neon_pack_chroma(accumulator);

    return result;
}

static sgl_neon_yuv_t sgl_neon_rgb_to_yuv(sgl_neon_rgb_t rgb)
{
    sgl_neon_yuv_t result;

    /*
     * Eight pixels use the same full-range Q8 matrix as the generic backend:
     * Y=(77R+150G+29B)/256, U=(-43R-85G+128B)/256+128,
     * V=(128R-107G-21B)/256+128.
     */
    result.y = sgl_neon_rgb_to_luma(rgb);
    result.u = sgl_neon_rgb_to_chroma(rgb, -43, -85, 128);
    result.v = sgl_neon_rgb_to_chroma(rgb, 128, -107, -21);

    return result;
}

static void sgl_neon_run_rgb_to_yuv444(
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
    sgl_neon_rgb_t rgb;
    sgl_neon_yuv_t yuv;

    pixel = start;
    end = start + count;
    while ((end - pixel) >= SGL_NEON_LANES) {
        rgb = sgl_neon_load_rgb(src, src_format, pixels, pixel);
        yuv = sgl_neon_rgb_to_yuv(rgb);
        sgl_neon_store_yuv444(dst, dst_format, pixels, pixel, yuv);
        pixel += SGL_NEON_LANES;
    }
    if (pixel < end) {
        /* Overlap the last full vector; no destination pixel has dependencies. */
        pixel = end - SGL_NEON_LANES;
        rgb = sgl_neon_load_rgb(src, src_format, pixels, pixel);
        yuv = sgl_neon_rgb_to_yuv(rgb);
        sgl_neon_store_yuv444(dst, dst_format, pixels, pixel, yuv);
    }
}

static void sgl_neon_yuv_run_rgb_to_422(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t x;
    sgl_int32_t end;
    sgl_size_t pixel;
    sgl_neon_rgb_t rgb;
    sgl_neon_yuv_t yuv;

    end = start + count;
    for (row = start; row < end; ++row) {
        /* Each store averages adjacent lane pairs into one 4:2:2 sample. */
        for (x = 0; x < data->width; x += (sgl_int32_t)SGL_NEON_LANES) {
            pixel = ((sgl_size_t)(sgl_uint32_t)row *
                     (sgl_size_t)(sgl_uint32_t)data->width) +
                (sgl_size_t)(sgl_uint32_t)x;
            rgb = sgl_neon_load_rgb(
                data->src, data->src_format, data->pixels, pixel);
            yuv = sgl_neon_rgb_to_yuv(rgb);
            sgl_neon_yuv_store_422(data, row, x, yuv);
        }
    }
}

static void sgl_neon_yuv_run_rgb_to_420(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t x;
    sgl_int32_t end;
    sgl_size_t pixel;
    sgl_neon_yuv_t top;
    sgl_neon_yuv_t bottom;

    end = start + count;
    for (row = start; row < end; row += 2) {
        /* Paired rows form the 2x2 chroma groups required by 4:2:0. */
        for (x = 0; x < data->width; x += (sgl_int32_t)SGL_NEON_LANES) {
            pixel = ((sgl_size_t)(sgl_uint32_t)row *
                     (sgl_size_t)(sgl_uint32_t)data->width) +
                (sgl_size_t)(sgl_uint32_t)x;
            top = sgl_neon_rgb_to_yuv(sgl_neon_load_rgb(
                data->src, data->src_format, data->pixels, pixel));
            bottom = sgl_neon_rgb_to_yuv(sgl_neon_load_rgb(
                data->src,
                data->src_format,
                data->pixels,
                pixel + (sgl_size_t)(sgl_uint32_t)data->width));
            sgl_neon_yuv_store_420(data, row, x, top, bottom);
        }
    }
}

static sgl_result_t sgl_neon_rgb_to_subsampled_yuv(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_neon_yuv_kernel_t kernel = sgl_neon_yuv_run_rgb_to_422;
    sgl_int32_t alignment = 1;
    sgl_result_t result = SGL_ERROR_NOT_SUPPORTED;

    if (sgl_neon_is_yuv420(dst_format) == SGL_TRUE) {
        kernel = sgl_neon_yuv_run_rgb_to_420;
        alignment = 2;
    }
    if ((sgl_neon_is_yuv422(dst_format) == SGL_TRUE) ||
        (sgl_neon_is_yuv420(dst_format) == SGL_TRUE)) {
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

sgl_result_t sgl_simd_rgb2yuv(
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
    if ((result == SGL_SUCCESS) &&
        (sgl_neon_is_rgb(src_format) != SGL_TRUE)) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
    if (result == SGL_SUCCESS) {
        result = sgl_neon_get_pixel_count(width, height, &pixels);
    }
    if (result == SGL_SUCCESS) {
        if ((pixels >= SGL_NEON_LANES) &&
            (sgl_neon_is_yuv444(dst_format) == SGL_TRUE)) {
            data = sgl_neon_convert_make_data(
                src,
                src_format,
                dst,
                dst_format,
                pixels,
                sgl_neon_run_rgb_to_yuv444);
            result = sgl_neon_convert_execute(pool, &data);
        }
        else {
            result = sgl_neon_rgb_to_subsampled_yuv(
                pool,
                src,
                src_format,
                dst,
                dst_format,
                width,
                height);
            if (result == SGL_ERROR_NOT_SUPPORTED) {
                /* Unaligned image edges are handled by the scalar backend. */
                result = sgl_generic_rgb2yuv(
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
