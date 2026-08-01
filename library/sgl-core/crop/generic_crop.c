/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "crop_common.h"

static void sgl_generic_crop_copy_row(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_size_t size)
{
    /* Copy exactly one packed row; source stride padding is not copied. */
    (void)sgl_memcpy(dst, src, size);
}

sgl_result_t sgl_generic_crop(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp)
{
    sgl_result_t result;

    result = sgl_crop_validate(
        src,
        dst,
        src_width,
        src_height,
        x,
        y,
        crop_width,
        crop_height,
        bpp);
    if (result == SGL_SUCCESS) {
        result = sgl_crop_run(
            pool,
            src,
            dst,
            src_width,
            x,
            y,
            crop_width,
            crop_height,
            bpp,
            sgl_generic_crop_copy_row);
    }

    return result;
}
