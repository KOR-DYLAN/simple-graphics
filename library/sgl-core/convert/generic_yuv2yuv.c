/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "generic_common.h"

static void sgl_convert_write_yuv_block_from_yuv(
    sgl_convert_request_t *request,
    sgl_convert_block_t block)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t count = 0;
    sgl_int32_t u_sum = 0;
    sgl_int32_t v_sum = 0;
    sgl_convert_yuv_t yuv;

    /*
     * Read source chroma at its native sampling rate, then average over the
     * destination group. This handles both downsampling and plane repacking.
     */
    for (y = block.y; y < (block.y + block.height); ++y) {
        for (x = block.x; x < (block.x + block.width); ++x) {
            yuv = sgl_convert_read_yuv(
                request->src,
                request->src_format,
                request->width,
                request->height,
                x,
                y);
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

static void sgl_convert_run_yuv_to_yuv(
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

    /* Keep each shared destination chroma sample inside one worker range. */
    for (y = start; y < end; y += group_height) {
        for (x = 0; x < request->width; x += group_width) {
            block = sgl_convert_make_block(request, x, y);
            sgl_convert_write_yuv_block_from_yuv(request, block);
        }
    }
}

sgl_result_t sgl_generic_yuv2yuv(
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
            sgl_convert_run_yuv_to_yuv);
        if (src_format == dst_format) {
            /* Preserve byte-exact data when no layout conversion is needed. */
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
