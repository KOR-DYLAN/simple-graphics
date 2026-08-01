/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "generic_common.h"

#define SGL_CONVERT_CHANNEL_MAX      (255)
#define SGL_CONVERT_CHROMA_OFFSET    (128)

static sgl_convert_rgb_t sgl_convert_yuv_to_rgb_pixel(sgl_convert_yuv_t yuv)
{
    sgl_convert_rgb_t result;
    sgl_int32_t y;
    sgl_int32_t u;
    sgl_int32_t v;

    y = (sgl_int32_t)yuv.y;
    u = (sgl_int32_t)yuv.u - SGL_CONVERT_CHROMA_OFFSET;
    v = (sgl_int32_t)yuv.v - SGL_CONVERT_CHROMA_OFFSET;

    /*
     * Inverse full-range Q8 conversion:
     *   R = Y + 359V / 256
     *   G = Y - (88U + 183V) / 256
     *   B = Y + 454U / 256
     * U and V above are centered signed chroma values.
     */
    result.r = sgl_convert_clip_int32(
        y + sgl_convert_divide_256(359 * v));
    result.g = sgl_convert_clip_int32(
        y - sgl_convert_divide_256((88 * u) + (183 * v)));
    result.b = sgl_convert_clip_int32(
        y + sgl_convert_divide_256(454 * u));
    result.a = (sgl_uint8_t)SGL_CONVERT_CHANNEL_MAX;

    return result;
}

static void sgl_convert_run_yuv_to_rgb(
    sgl_convert_request_t *request,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t end;
    sgl_convert_yuv_t yuv;
    sgl_convert_rgb_t rgb;

    end = start + count;
    for (y = start; y < end; ++y) {
        for (x = 0; x < request->width; ++x) {
            /* Subsampled readers expand the shared chroma for this pixel. */
            yuv = sgl_convert_read_yuv(
                request->src,
                request->src_format,
                request->width,
                request->height,
                x,
                y);
            rgb = sgl_convert_yuv_to_rgb_pixel(yuv);
            sgl_convert_write_rgb(
                request->dst,
                request->dst_format,
                request->width,
                request->height,
                x,
                y,
                rgb);
        }
    }
}

sgl_result_t sgl_generic_yuv2rgb(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_result_t result;
    sgl_convert_request_t request;

    result = sgl_convert_validate_common(src, dst, width, height);
    if ((result == SGL_SUCCESS) &&
        ((sgl_convert_is_yuv_format(src_format) != SGL_TRUE) ||
         (sgl_convert_is_rgb_format(dst_format) != SGL_TRUE))) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
    if (result == SGL_SUCCESS) {
        result = sgl_convert_validate_buffer_sizes(
            src_format, dst_format, width, height);
    }
    if (result == SGL_SUCCESS) {
        request = sgl_convert_make_request(
            src,
            src_format,
            dst,
            dst_format,
            width,
            height,
            1,
            sgl_convert_run_yuv_to_rgb);
        result = sgl_convert_run(pool, &request);
    }

    return result;
}
