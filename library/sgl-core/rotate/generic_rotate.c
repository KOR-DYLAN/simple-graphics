/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include "rotate_common.h"

#include <sgl_memory_cast.h>

static sgl_rotate_lookup_t *sgl_rotate_lookup_from_memory(void *memory)
{
    sgl_rotate_lookup_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic allocator storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_rotate_lookup_t *)memory;

    return result;
}

static void sgl_rotate_initialize_lut_row(
    const sgl_rotate_context_t *context,
    sgl_rotate_lookup_t *lut,
    sgl_int32_t dst_y)
{
    sgl_rotate_row_start_t position;
    sgl_int32_t dst_x;
    sgl_int32_t valid_start = -1;
    sgl_int32_t valid_end = 0;
    sgl_size_t dst_pixel;

    /* Cache nearest-neighbor source pixel indexes for one destination row. */
    position = sgl_rotate_get_row_start(context, dst_y);
    dst_pixel = (sgl_size_t)(sgl_uint32_t)dst_y *
        (sgl_size_t)(sgl_uint32_t)context->dst_width;
    for (dst_x = 0; dst_x < context->dst_width; ++dst_x) {
        lut->source_offsets[dst_pixel] = sgl_rotate_get_source_offset(
            context, position.x, position.y);
        if (lut->source_offsets[dst_pixel] >= 0) {
            if (valid_start < 0) {
                valid_start = dst_x;
            }
            valid_end = dst_x + 1;
        }
        position.x += (sgl_q16_ext_t)context->cos_angle;
        position.y -= (sgl_q16_ext_t)context->sin_angle;
        ++dst_pixel;
    }
    if (valid_start < 0) {
        valid_start = 0;
    }
    lut->row_ranges[(sgl_size_t)(sgl_uint32_t)dst_y << 1U] = valid_start;
    lut->row_ranges[
        ((sgl_size_t)(sgl_uint32_t)dst_y << 1U) + 1U] = valid_end;
}

static void sgl_rotate_initialize_lut(
    const sgl_rotate_context_t *context,
    sgl_rotate_lookup_t *lut)
{
    sgl_int32_t row;

    lut->src_width = context->src_width;
    lut->src_height = context->src_height;
    lut->dst_width = context->dst_width;
    lut->dst_height = context->dst_height;
    lut->normalized_angle = context->normalized_angle;
    /* Row-major entries match the destination's row-major pixel order. */
    for (row = 0; row < context->dst_height; ++row) {
        sgl_rotate_initialize_lut_row(context, lut, row);
    }
}

sgl_rotate_lookup_t *sgl_generic_create_rotate_lut(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_float64_t angle)
{
    sgl_rotate_context_t context;
    sgl_rotate_lookup_t *lut = SGL_NULL;

    if (sgl_rotate_context_initialize(
            src_width,
            src_height,
            SGL_BPP8,
            angle,
            &context) ==
        SGL_SUCCESS) {
        /* The LUT object and its offset array have independent lifetimes. */
        lut = sgl_rotate_lookup_from_memory(
            sgl_calloc(1U, sizeof(sgl_rotate_lookup_t)));
        if (lut != SGL_NULL) {
            /* With BPP8, dst_bytes is exactly the destination pixel count. */
            lut->source_offsets = sgl_memory_as_int32(
                sgl_calloc(
                    context.dst_bytes,
                    sizeof(sgl_int32_t)));
            lut->row_ranges = sgl_memory_as_int32(
                sgl_calloc(
                    (sgl_size_t)(sgl_uint32_t)context.dst_height,
                    sizeof(sgl_int32_t) * 2U));
            if ((lut->source_offsets != SGL_NULL) &&
                (lut->row_ranges != SGL_NULL)) {
                sgl_rotate_initialize_lut(&context, lut);
            }
            else {
                SGL_SAFE_FREE(lut->source_offsets);
                SGL_SAFE_FREE(lut->row_ranges);
                sgl_free(lut);
                lut = SGL_NULL;
            }
        }
    }

    return lut;
}

void sgl_generic_destroy_rotate_lut(sgl_rotate_lookup_t *lut)
{
    if (lut != SGL_NULL) {
        SGL_SAFE_FREE(lut->source_offsets);
        SGL_SAFE_FREE(lut->row_ranges);
        sgl_free(lut);
    }
}

sgl_result_t sgl_get_rotated_dimensions(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_float64_t angle,
    sgl_int32_t *dst_width,
    sgl_int32_t *dst_height)
{
    sgl_rotate_context_t context;
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    if ((dst_width != SGL_NULL) && (dst_height != SGL_NULL)) {
        result = sgl_rotate_context_initialize(
            src_width,
            src_height,
            SGL_BPP8,
            angle,
            &context);
        if (result == SGL_SUCCESS) {
            *dst_width = context.dst_width;
            *dst_height = context.dst_height;
        }
    }

    return result;
}

sgl_int32_t sgl_get_rotated_size(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t bpp,
    sgl_float64_t angle)
{
    sgl_rotate_context_t context;
    sgl_int32_t result = 0;

    if (sgl_rotate_context_initialize(
            src_width,
            src_height,
            bpp,
            angle,
            &context) == SGL_SUCCESS) {
        result = (sgl_int32_t)context.dst_bytes;
    }

    return result;
}

sgl_result_t sgl_generic_rotate(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_rotate_lookup_t *SGL_RESTRICT ext_lut,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp,
    sgl_float64_t angle)
{
    sgl_rotate_context_t context;
    sgl_result_t result;

    result = sgl_rotate_validate_buffers(src, dst);
    if (result == SGL_SUCCESS) {
        result = sgl_rotate_context_initialize(
            src_width,
            src_height,
            bpp,
            angle,
            &context);
    }
    if (result == SGL_SUCCESS) {
        result = sgl_rotate_validate_lut(&context, ext_lut);
    }
    if (result == SGL_SUCCESS) {
        /*
         * dst is caller-owned. Rotation never allocates or replaces the
         * destination buffer.
         */
        result = sgl_rotate_run_scalar(
            pool,
            &context,
            src,
            dst,
            ext_lut);
    }

    return result;
}
