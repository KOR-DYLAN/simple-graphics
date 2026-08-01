/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "generic_common.h"

#define SGL_CONVERT_CHROMA_OFFSET    (128)

static sgl_convert_yuv_t sgl_convert_rgb_to_yuv_pixel(sgl_convert_rgb_t rgb)
{
    sgl_convert_yuv_t result;
    sgl_int32_t red;
    sgl_int32_t green;
    sgl_int32_t blue;

    red = (sgl_int32_t)rgb.r;
    green = (sgl_int32_t)rgb.g;
    blue = (sgl_int32_t)rgb.b;

    /*
     * Full-range BT.601-style coefficients are represented in Q8:
     *   Y = ( 77R + 150G +  29B) / 256
     *   U = (-43R -  85G + 128B) / 256 + 128
     *   V = (128R - 107G -  21B) / 256 + 128
     * Integer division preserves C99 truncation toward zero.
     */
    result.y = sgl_convert_clip_int32(sgl_convert_divide_256(
        (77 * red) + (150 * green) + (29 * blue)));
    result.u = sgl_convert_clip_int32(
        sgl_convert_divide_256(
            (-43 * red) - (85 * green) + (128 * blue)) +
        SGL_CONVERT_CHROMA_OFFSET);
    result.v = sgl_convert_clip_int32(
        sgl_convert_divide_256(
            (128 * red) - (107 * green) - (21 * blue)) +
        SGL_CONVERT_CHROMA_OFFSET);

    return result;
}

static void sgl_convert_write_yuv_block_from_rgb(
    sgl_convert_request_t *request,
    sgl_convert_block_t block)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t count = 0;
    sgl_int32_t u_sum = 0;
    sgl_int32_t v_sum = 0;
    sgl_convert_rgb_t rgb;
    sgl_convert_yuv_t yuv;

    /*
     * Luma remains per-pixel. U and V are accumulated over the destination
     * chroma group: 1x1 for 4:4:4, 2x1 for 4:2:2, and 2x2 for 4:2:0.
     */
    for (y = block.y; y < (block.y + block.height); ++y) {
        for (x = block.x; x < (block.x + block.width); ++x) {
            rgb = sgl_convert_read_rgb(
                request->src,
                request->src_format,
                request->width,
                request->height,
                x,
                y);
            yuv = sgl_convert_rgb_to_yuv_pixel(rgb);
            sgl_convert_write_yuv_luma(
                request->dst,
                request->dst_format,
                request->width,
                request->height,
                x,
                y,
                yuv.y);
            u_sum += (sgl_int32_t)yuv.u;
            v_sum += (sgl_int32_t)yuv.v;
            ++count;
        }
    }

    yuv.y = 0U;
    yuv.u = sgl_convert_average_chroma(u_sum, count);
    yuv.v = sgl_convert_average_chroma(v_sum, count);
    sgl_convert_write_yuv_chroma(
        request->dst,
        request->dst_format,
        request->width,
        request->height,
        block.x,
        block.y,
        yuv);
}

static void sgl_convert_run_rgb_to_yuv(
    sgl_convert_request_t *request,
    sgl_int32_t start,
    sgl_int32_t count)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t group_width;
    sgl_int32_t group_height;
    sgl_int32_t end;
    sgl_convert_block_t block;

    group_width = sgl_convert_yuv_group_width(request->dst_format);
    group_height = sgl_convert_yuv_group_height(request->dst_format);
    end = start + count;

    /* Row ranges are aligned to group_height by the common dispatcher. */
    for (y = start; y < end; y += group_height) {
        for (x = 0; x < request->width; x += group_width) {
            block = sgl_convert_make_block(request, x, y);
            sgl_convert_write_yuv_block_from_rgb(request, block);
        }
    }
}

sgl_result_t sgl_generic_rgb2yuv(
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
         (sgl_convert_is_yuv_format(dst_format) != SGL_TRUE))) {
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
            sgl_convert_yuv_group_height(dst_format),
            sgl_convert_run_rgb_to_yuv);
        result = sgl_convert_run(pool, &request);
    }

    return result;
}
