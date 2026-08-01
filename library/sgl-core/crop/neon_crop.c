/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <arm_neon.h>

#include "crop_common.h"

#define SGL_NEON_CROP_VECTOR_BYTES  (16U)
#define SGL_NEON_CROP_BULK_BYTES    (64U)

static void sgl_neon_crop_copy_row(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_size_t size)
{
    sgl_size_t offset = 0U;
    uint8x16_t value0;
    uint8x16_t value1;
    uint8x16_t value2;
    uint8x16_t value3;

    /* Four 16-byte vectors reduce loop and branch overhead for full blocks. */
    while ((size - offset) >= SGL_NEON_CROP_BULK_BYTES) {
        value0 = vld1q_u8(&src[offset]);
        value1 = vld1q_u8(&src[offset + 16U]);
        value2 = vld1q_u8(&src[offset + 32U]);
        value3 = vld1q_u8(&src[offset + 48U]);
        vst1q_u8(&dst[offset], value0);
        vst1q_u8(&dst[offset + 16U], value1);
        vst1q_u8(&dst[offset + 32U], value2);
        vst1q_u8(&dst[offset + 48U], value3);
        offset += SGL_NEON_CROP_BULK_BYTES;
    }
    /* Copy any remaining complete 16-byte vector. */
    while ((size - offset) >= SGL_NEON_CROP_VECTOR_BYTES) {
        value0 = vld1q_u8(&src[offset]);
        vst1q_u8(&dst[offset], value0);
        offset += SGL_NEON_CROP_VECTOR_BYTES;
    }
    /* The scalar tail avoids reading or writing beyond the requested row. */
    while (offset < size) {
        dst[offset] = src[offset];
        ++offset;
    }
}

sgl_result_t sgl_simd_crop(
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
            sgl_neon_crop_copy_row);
    }

    return result;
}
