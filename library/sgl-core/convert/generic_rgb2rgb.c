/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "generic_common.h"

static void sgl_convert_run_rgb_to_rgb(
    sgl_convert_request_t *request,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t end;
    sgl_convert_rgb_t pixel;

    end = start + count;
    for (y = start; y < end; ++y) {
        for (x = 0; x < request->width; ++x) {
            /* Readers and writers normalize every packed or planar order. */
            pixel = sgl_convert_read_rgb(
                request->src,
                request->src_format,
                request->width,
                request->height,
                x,
                y);
            sgl_convert_write_rgb(
                request->dst,
                request->dst_format,
                request->width,
                request->height,
                x,
                y,
                pixel);
        }
    }
}

sgl_result_t sgl_generic_rgb2rgb(
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
        ((sgl_convert_is_rgb_format(src_format) != SGL_TRUE) ||
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
            sgl_convert_run_rgb_to_rgb);
        if (src_format == dst_format) {
            /* Identical layouts need no per-channel interpretation. */
            (void)sgl_memcpy(
                dst,
                src,
                sgl_convert_buffer_size(src_format, width, height));
        }
        else {
            result = sgl_convert_run(pool, &request);
        }
    }

    return result;
}
