/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-fixed_point.h>

#include "neon_common.h"

#define SGL_NEON_CONVERT_CHROMA_OFFSET    (128)
#define SGL_NEON_CONVERT_CHANNEL_MAX      (255U)

static int32x4_t sgl_neon_yuv_to_green_half(
    int16x4_t y,
    int16x4_t u,
    int16x4_t v);
static int16x8_t sgl_neon_center_chroma(uint8x8_t value);

static int32x4_t sgl_neon_divide_256_s32(int32x4_t value)
{
    int32x4_t negative;
    int32x4_t correction;
    int32x4_t adjusted;
    int32x4_t result;

    negative = vshrq_n_s32(value, 31);
    correction = vandq_s32(negative, vdupq_n_s32(255));
    adjusted = vaddq_s32(value, correction);
    /*
     * Remove eight coefficient fractional bits. Adding 255 only to negative
     * lanes changes arithmetic-shift floor into C99 truncation toward zero.
     */
    result = vshrq_n_s32(adjusted, 8);

    return result;
}

static uint8x8_t sgl_neon_yuv_to_red(
    int16x8_t y,
    int16x8_t v)
{
    int32x4_t low;
    int32x4_t high;
    uint8x8_t result;

    low = vmull_n_s16(vget_low_s16(v), 359);
    high = vmull_n_s16(vget_high_s16(v), 359);
    low = sgl_neon_divide_256_s32(low);
    high = sgl_neon_divide_256_s32(high);
    low = vaddw_s16(low, vget_low_s16(y));
    high = vaddw_s16(high, vget_high_s16(y));
    result = sgl_simd_clamp_u8_i32(low, high);

    return result;
}

static uint8x8_t sgl_neon_yuv_to_green(
    int16x8_t y,
    int16x8_t u,
    int16x8_t v)
{
    int32x4_t low;
    int32x4_t high;
    uint8x8_t result;

    low = sgl_neon_yuv_to_green_half(
        vget_low_s16(y), vget_low_s16(u), vget_low_s16(v));
    high = sgl_neon_yuv_to_green_half(
        vget_high_s16(y), vget_high_s16(u), vget_high_s16(v));
    result = sgl_simd_clamp_u8_i32(low, high);

    return result;
}

static int32x4_t sgl_neon_yuv_to_green_half(
    int16x4_t y,
    int16x4_t u,
    int16x4_t v)
{
    int32x4_t low;
    int32x4_t result;

    low = vmull_n_s16(u, 88);
    low = vmlal_n_s16(low, v, 183);
    low = sgl_neon_divide_256_s32(low);
    low = vsubw_s16(low, y);
    low = vnegq_s32(low);
    result = low;

    return result;
}

static uint8x8_t sgl_neon_yuv_to_blue(
    int16x8_t y,
    int16x8_t u)
{
    int32x4_t low;
    int32x4_t high;
    uint8x8_t result;

    low = vmull_n_s16(vget_low_s16(u), 454);
    high = vmull_n_s16(vget_high_s16(u), 454);
    low = sgl_neon_divide_256_s32(low);
    high = sgl_neon_divide_256_s32(high);
    low = vaddw_s16(low, vget_low_s16(y));
    high = vaddw_s16(high, vget_high_s16(y));
    result = sgl_simd_clamp_u8_i32(low, high);

    return result;
}

static sgl_neon_rgb_t sgl_neon_yuv_to_rgb(sgl_neon_yuv_t yuv)
{
    int16x8_t y;
    int16x8_t u;
    int16x8_t v;
    sgl_neon_rgb_t result;

    /*
     * Apply the inverse Q8 matrix to eight pixels. U and V are centered before
     * multiplying, and each 32-bit result is clamped back to an 8-bit channel.
     */
    y = sgl_neon_widen_u8_to_s16(yuv.y);
    u = sgl_neon_center_chroma(yuv.u);
    v = sgl_neon_center_chroma(yuv.v);

    result.r = sgl_neon_yuv_to_red(y, v);
    result.g = sgl_neon_yuv_to_green(y, u, v);
    result.b = sgl_neon_yuv_to_blue(y, u);
    result.a = vdup_n_u8(SGL_NEON_CONVERT_CHANNEL_MAX);

    return result;
}

static int16x8_t sgl_neon_center_chroma(uint8x8_t value)
{
    int16x8_t widened;
    int16x8_t result;

    widened = sgl_neon_widen_u8_to_s16(value);
    result = vsubq_s16(
        widened, vdupq_n_s16(SGL_NEON_CONVERT_CHROMA_OFFSET));

    return result;
}

static void sgl_neon_run_yuv444_to_rgb(
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
        yuv = sgl_neon_load_yuv444(src, src_format, pixels, pixel);
        rgb = sgl_neon_yuv_to_rgb(yuv);
        sgl_neon_store_rgb(dst, dst_format, pixels, pixel, rgb);
        pixel += SGL_NEON_LANES;
    }
    if (pixel < end) {
        /* Overlap the last full vector to avoid a separate scalar tail. */
        pixel = end - SGL_NEON_LANES;
        yuv = sgl_neon_load_yuv444(src, src_format, pixels, pixel);
        rgb = sgl_neon_yuv_to_rgb(yuv);
        sgl_neon_store_rgb(dst, dst_format, pixels, pixel, rgb);
    }
}

static void sgl_neon_yuv_run_to_rgb(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t row;
    sgl_int32_t x;
    sgl_int32_t end;
    sgl_size_t pixel;
    sgl_neon_rgb_t rgb;

    end = start + count;
    for (row = start; row < end; ++row) {
        for (x = 0; x < data->width; x += (sgl_int32_t)SGL_NEON_LANES) {
            pixel = ((sgl_size_t)(sgl_uint32_t)row *
                     (sgl_size_t)(sgl_uint32_t)data->width) +
                (sgl_size_t)(sgl_uint32_t)x;
            rgb = sgl_neon_yuv_to_rgb(sgl_neon_yuv_load(data, row, x));
            sgl_neon_store_rgb(
                data->dst, data->dst_format, data->pixels, pixel, rgb);
        }
    }
}

static sgl_result_t sgl_neon_subsampled_yuv_to_rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_result_t result = SGL_ERROR_NOT_SUPPORTED;

    if ((sgl_neon_is_yuv422(src_format) == SGL_TRUE) ||
        (sgl_neon_is_yuv420(src_format) == SGL_TRUE)) {
        result = sgl_neon_yuv_run(
            pool,
            src,
            src_format,
            dst,
            dst_format,
            width,
            height,
            1,
            sgl_neon_yuv_run_to_rgb);
    }

    return result;
}

sgl_result_t sgl_simd_yuv2rgb(
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
        (sgl_neon_is_rgb(dst_format) != SGL_TRUE)) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
    if (result == SGL_SUCCESS) {
        result = sgl_neon_get_pixel_count(width, height, &pixels);
    }
    if (result == SGL_SUCCESS) {
        if ((pixels >= SGL_NEON_LANES) &&
            (sgl_neon_is_yuv444(src_format) == SGL_TRUE)) {
            data = sgl_neon_convert_make_data(
                src,
                src_format,
                dst,
                dst_format,
                pixels,
                sgl_neon_run_yuv444_to_rgb);
            result = sgl_neon_convert_execute(pool, &data);
        }
        else {
            result = sgl_neon_subsampled_yuv_to_rgb(
                pool,
                src,
                src_format,
                dst,
                dst_format,
                width,
                height);
            if (result == SGL_ERROR_NOT_SUPPORTED) {
                /* Generic access handles widths that do not fill a vector. */
                result = sgl_generic_yuv2rgb(
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
