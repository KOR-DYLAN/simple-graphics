/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "neon_common.h"

static void sgl_neon_run_rgb_to_rgb(
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
    sgl_neon_rgb_t value;

    pixel = start;
    end = start + count;
    while ((end - pixel) >= SGL_NEON_LANES) {
        value = sgl_neon_load_rgb(src, src_format, pixels, pixel);
        sgl_neon_store_rgb(dst, dst_format, pixels, pixel, value);
        pixel += SGL_NEON_LANES;
    }
    if (pixel < end) {
        /*
         * Reprocess the final complete vector instead of using a scalar tail.
         * The overlap is safe because each destination pixel is independent.
         */
        pixel = end - SGL_NEON_LANES;
        value = sgl_neon_load_rgb(src, src_format, pixels, pixel);
        sgl_neon_store_rgb(dst, dst_format, pixels, pixel, value);
    }
}

sgl_result_t sgl_simd_rgb2rgb(
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
        ((sgl_neon_is_rgb(src_format) != SGL_TRUE) ||
         (sgl_neon_is_rgb(dst_format) != SGL_TRUE))) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
    if ((result == SGL_SUCCESS) && (src_format == dst_format)) {
        /*
         * An identical layout is a byte copy.  Reusing the generic fast path
         * avoids NEON channel deinterleave/interleave work; sgl_memcpy itself
         * selects the accelerated memory backend on NEON builds.
         */
        result = sgl_generic_rgb2rgb(
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
        if (pixels >= SGL_NEON_LANES) {
            data = sgl_neon_convert_make_data(
                src,
                src_format,
                dst,
                dst_format,
                pixels,
                sgl_neon_run_rgb_to_rgb);
            result = sgl_neon_convert_execute(pool, &data);
        }
        else {
            result = sgl_generic_rgb2rgb(
                pool,
                src,
                src_format,
                dst,
                dst_format,
                width,
                height);
        }
    }

    return result;
}
