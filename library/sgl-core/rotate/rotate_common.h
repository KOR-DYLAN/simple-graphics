/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_ROTATE_COMMON_H_
#define SGL_ROTATE_COMMON_H_

#include <sgl-core.h>
#include <sgl-fixed_point.h>

#define SGL_ROTATE_INVALID_OFFSET  (-1)

/* Precomputed Q16 geometry shared by scalar and NEON entry points. */
typedef struct {
    sgl_int32_t src_width;
    sgl_int32_t src_height;
    sgl_int32_t dst_width;
    sgl_int32_t dst_height;
    sgl_int32_t bpp;
    sgl_q16_t sin_angle;
    sgl_q16_t cos_angle;
    sgl_q16_t normalized_angle;
    sgl_size_t dst_bytes;
} sgl_rotate_context_t;

/* Q16 source coordinate corresponding to destination x == 0 for one row. */
typedef struct {
    sgl_q16_ext_t x;
    sgl_q16_ext_t y;
} sgl_rotate_row_start_t;

/*
 * LUT entries are source pixel indexes, not byte offsets. An entry of -1
 * marks a destination pixel outside the source image. row_ranges stores the
 * inclusive start and exclusive end of the valid interval for every row.
 */
struct sgl_rotate_lookup_table {
    sgl_int32_t src_width;
    sgl_int32_t src_height;
    sgl_int32_t dst_width;
    sgl_int32_t dst_height;
    sgl_q16_t normalized_angle;
    sgl_int32_t *SGL_RESTRICT source_offsets;
    sgl_int32_t *SGL_RESTRICT row_ranges;
};

/* Validate dimensions and convert the input angle to normalized Q16 state. */
sgl_result_t sgl_rotate_context_initialize(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t bpp,
    sgl_float64_t angle,
    sgl_rotate_context_t *context);

sgl_result_t sgl_rotate_validate_buffers(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst);

sgl_result_t sgl_rotate_validate_lut(
    const sgl_rotate_context_t *context,
    const sgl_rotate_lookup_t *lut);

/* Coordinate helpers shared by direct mapping and LUT construction. */
sgl_rotate_row_start_t sgl_rotate_get_row_start(
    const sgl_rotate_context_t *context,
    sgl_int32_t dst_y);

static SGL_ALWAYS_INLINE sgl_int32_t sgl_rotate_get_source_offset(
    const sgl_rotate_context_t *context,
    sgl_q16_ext_t source_x,
    sgl_q16_ext_t source_y)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t result = SGL_ROTATE_INVALID_OFFSET;

    /* Nearest-neighbor sampling rounds Q16 coordinates to source pixels. */
    x = sgl_q16_round_to_int32(source_x);
    y = sgl_q16_round_to_int32(source_y);
    if ((x >= 0) &&
        (y >= 0) &&
        (x < context->src_width) &&
        (y < context->src_height)) {
        result = (sgl_int32_t)(
            ((sgl_size_t)(sgl_uint32_t)y *
             (sgl_size_t)(sgl_uint32_t)context->src_width) +
            (sgl_size_t)(sgl_uint32_t)x);
    }

    return result;
}

/* Scalar nearest-neighbor mapper, optionally driven by a caller-owned LUT. */
sgl_result_t sgl_rotate_run_scalar(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut);

#endif  /* SGL_ROTATE_COMMON_H_ */
