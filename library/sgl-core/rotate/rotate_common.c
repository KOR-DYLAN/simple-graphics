/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "rotate_common.h"

#include "../threadpool/parallel_for.h"

#define SGL_ROTATE_FULL_CIRCLE_Q16          (23592960)
#define SGL_ROTATE_HALF_CIRCLE_Q16          (11796480)
#define SGL_ROTATE_RIGHT_ANGLE_Q16          (5898240)
#define SGL_ROTATE_THREE_QUARTER_Q16        (17694720)
#define SGL_ROTATE_CORDIC_GAIN_Q16          (39797)
#define SGL_ROTATE_INT32_MAX                ((sgl_size_t)2147483647)
#define SGL_ROTATE_ANGLE_LIMIT              (2147483647.0)
#define SGL_ROTATE_CORDIC_STEPS             (16)
#if defined(SGL_CFG_HAS_THREAD)
#define SGL_ROTATE_MIN_PARALLEL_PIXELS      ((sgl_uint64_t)65536U)
#endif

typedef struct {
    const sgl_rotate_context_t *context;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    const sgl_rotate_lookup_t *lut;
} sgl_rotate_data_t;

typedef struct {
    sgl_int32_t start;
    sgl_int32_t end;
} sgl_rotate_valid_range_t;

static sgl_bool_t sgl_rotate_angle_is_valid(sgl_float64_t angle)
{
    sgl_bool_t result = SGL_FALSE;

    if ((angle == angle) &&
        (angle >= -SGL_ROTATE_ANGLE_LIMIT) &&
        (angle <= SGL_ROTATE_ANGLE_LIMIT)) {
        result = SGL_TRUE;
    }

    return result;
}

/*
 * Floating point is confined to the public angle boundary. Normalize degrees
 * to [0, 360), then convert once to Q16 for all per-pixel calculations.
 */
static sgl_q16_t sgl_rotate_angle_to_q16(sgl_float64_t angle)
{
    sgl_int32_t turns;
    sgl_float64_t normalized;
    sgl_float64_t scaled;
    sgl_q16_t result;

    turns = (sgl_int32_t)(angle / 360.0);
    normalized = angle - ((sgl_float64_t)turns * 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }

    scaled = (normalized * (sgl_float64_t)SGL_Q16_ONE) + 0.5;
    result = (sgl_q16_t)scaled;
    if (result >= SGL_ROTATE_FULL_CIRCLE_Q16) {
        result = 0;
    }

    return result;
}

static sgl_q16_t sgl_rotate_abs_q16(sgl_q16_t value)
{
    sgl_q16_t result = value;

    if (value < 0) {
        result = -value;
    }

    return result;
}

static void sgl_rotate_cordic(
    sgl_q16_t normalized_angle,
    sgl_q16_t *sin_angle,
    sgl_q16_t *cos_angle)
{
    /* atan(2^-index), expressed in Q16 degrees for sixteen CORDIC steps. */
    static const sgl_q16_t cordic_angles[SGL_ROTATE_CORDIC_STEPS] = {
        2949120, 1740967, 919879, 466945,
        234379, 117304, 58666, 29335,
        14668, 7334, 3667, 1833,
        917, 458, 229, 115
    };
    sgl_int32_t index;
    sgl_int32_t sign = 1;
    sgl_q16_ext_t x;
    sgl_q16_ext_t y;
    sgl_q16_ext_t next_x;
    sgl_q16_ext_t z;

    z = (sgl_q16_ext_t)normalized_angle;
    if (z > (sgl_q16_ext_t)SGL_ROTATE_HALF_CIRCLE_Q16) {
        z -= (sgl_q16_ext_t)SGL_ROTATE_FULL_CIRCLE_Q16;
    }
    if (z > (sgl_q16_ext_t)SGL_ROTATE_RIGHT_ANGLE_Q16) {
        z -= (sgl_q16_ext_t)SGL_ROTATE_HALF_CIRCLE_Q16;
        sign = -1;
    }
    else if (z < -(sgl_q16_ext_t)SGL_ROTATE_RIGHT_ANGLE_Q16) {
        z += (sgl_q16_ext_t)SGL_ROTATE_HALF_CIRCLE_Q16;
        sign = -1;
    }
    else {
        sign = 1;
    }

    /* 0.607252935 in Q16 compensates for the accumulated CORDIC gain. */
    x = (sgl_q16_ext_t)SGL_ROTATE_CORDIC_GAIN_Q16;
    y = 0;
    for (index = 0; index < SGL_ROTATE_CORDIC_STEPS; ++index) {
        sgl_q16_ext_t x_shift;
        sgl_q16_ext_t y_shift;

        /*
         * A shift by index divides the Q16 vector by 2^index, which is the
         * CORDIC rotation step. The helper keeps negative shifts portable.
         */
        x_shift = sgl_fixed_div_pow2_i64(x, (sgl_uint32_t)index);
        y_shift = sgl_fixed_div_pow2_i64(y, (sgl_uint32_t)index);
        if (z >= 0) {
            next_x = x - y_shift;
            y += x_shift;
            z -= (sgl_q16_ext_t)cordic_angles[index];
        }
        else {
            next_x = x + y_shift;
            y -= x_shift;
            z += (sgl_q16_ext_t)cordic_angles[index];
        }
        x = next_x;
    }

    x *= (sgl_q16_ext_t)sign;
    y *= (sgl_q16_ext_t)sign;
    *cos_angle = (sgl_q16_t)x;
    *sin_angle = (sgl_q16_t)y;
}

static void sgl_rotate_get_trig(
    sgl_q16_t normalized_angle,
    sgl_q16_t *sin_angle,
    sgl_q16_t *cos_angle)
{
    switch (normalized_angle) {
    case 0:
        *sin_angle = 0;
        *cos_angle = (sgl_q16_t)SGL_Q16_ONE;
        break;
    case SGL_ROTATE_RIGHT_ANGLE_Q16:
        *sin_angle = (sgl_q16_t)SGL_Q16_ONE;
        *cos_angle = 0;
        break;
    case SGL_ROTATE_HALF_CIRCLE_Q16:
        *sin_angle = 0;
        *cos_angle = (sgl_q16_t)-SGL_Q16_ONE;
        break;
    case SGL_ROTATE_THREE_QUARTER_Q16:
        *sin_angle = (sgl_q16_t)-SGL_Q16_ONE;
        *cos_angle = 0;
        break;
    default:
        sgl_rotate_cordic(normalized_angle, sin_angle, cos_angle);
        break;
    }
}

static sgl_int32_t sgl_rotate_ceil_q16_positive(sgl_q16_ext_t value)
{
    sgl_uint64_t rounded;
    sgl_int32_t result = 0;

    /*
     * Adding the low sixteen-bit mask before shifting implements ceil for a
     * nonnegative Q16 value.
     */
    rounded = (sgl_uint64_t)value + (sgl_uint64_t)SGL_Q16_MASK;
    rounded >>= SGL_Q16_FRAC_BITS;
    if (rounded <= (sgl_uint64_t)SGL_ROTATE_INT32_MAX) {
        result = (sgl_int32_t)rounded;
    }

    return result;
}

static void sgl_rotate_calculate_size(sgl_rotate_context_t *context)
{
    sgl_q16_ext_t abs_sin;
    sgl_q16_ext_t abs_cos;
    sgl_q16_ext_t width;
    sgl_q16_ext_t height;

    abs_sin = (sgl_q16_ext_t)sgl_rotate_abs_q16(context->sin_angle);
    abs_cos = (sgl_q16_ext_t)sgl_rotate_abs_q16(context->cos_angle);
    /* Bounding box: W' = ceil(W|cos| + H|sin|), H' swaps sin and cos. */
    width = ((sgl_q16_ext_t)context->src_width * abs_cos) +
        ((sgl_q16_ext_t)context->src_height * abs_sin);
    height = ((sgl_q16_ext_t)context->src_width * abs_sin) +
        ((sgl_q16_ext_t)context->src_height * abs_cos);

    context->dst_width = sgl_rotate_ceil_q16_positive(width);
    context->dst_height = sgl_rotate_ceil_q16_positive(height);
}

/* Checked multiplication prevents wrapped image sizes and byte offsets. */
static sgl_bool_t sgl_rotate_multiply_size(
    sgl_size_t left,
    sgl_size_t right,
    sgl_size_t *value)
{
    sgl_bool_t result = SGL_FALSE;

    if ((value != SGL_NULL) &&
        (right > 0U) &&
        (left <= (SGL_SIZE_MAX / right))) {
        *value = left * right;
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_rotate_calculate_bytes(sgl_rotate_context_t *context)
{
    sgl_size_t pixels;
    sgl_size_t bytes;
    sgl_bool_t result = SGL_FALSE;

    if ((context->dst_width > 0) && (context->dst_height > 0)) {
        sgl_size_t width;
        sgl_size_t height;
        sgl_size_t bpp;

        width = (sgl_size_t)(sgl_uint32_t)context->dst_width;
        height = (sgl_size_t)(sgl_uint32_t)context->dst_height;
        bpp = (sgl_size_t)(sgl_uint32_t)context->bpp;
        if (sgl_rotate_multiply_size(width, height, &pixels) == SGL_TRUE) {
            if ((sgl_rotate_multiply_size(pixels, bpp, &bytes) == SGL_TRUE) &&
                (bytes <= SGL_ROTATE_INT32_MAX)) {
                context->dst_bytes = bytes;
                result = SGL_TRUE;
            }
        }
    }

    return result;
}

sgl_rotate_row_start_t sgl_rotate_get_row_start(
    const sgl_rotate_context_t *context,
    sgl_int32_t dst_y)
{
    sgl_rotate_row_start_t result;
    sgl_q16_ext_t rel_x2;
    sgl_q16_ext_t rel_y2;
    sgl_q16_ext_t src_center_x2;
    sgl_q16_ext_t src_center_y2;
    sgl_int32_t dst_width_minus_one;
    sgl_int32_t dst_height_minus_one;
    sgl_int32_t src_width_minus_one;
    sgl_int32_t src_height_minus_one;

    dst_width_minus_one = context->dst_width - 1;
    dst_height_minus_one = context->dst_height - 1;
    src_width_minus_one = context->src_width - 1;
    src_height_minus_one = context->src_height - 1;
    rel_x2 = -(sgl_q16_ext_t)dst_width_minus_one;
    rel_y2 = ((sgl_q16_ext_t)dst_y * 2) -
        (sgl_q16_ext_t)dst_height_minus_one;
    src_center_x2 =
        (sgl_q16_ext_t)src_width_minus_one * SGL_Q16_ONE;
    src_center_y2 =
        (sgl_q16_ext_t)src_height_minus_one * SGL_Q16_ONE;

    /*
     * Inverse-map the destination row's left pixel into source coordinates.
     * rel_x2/rel_y2 use half-pixel units so centers remain exact integers.
     * Dividing the accumulated numerator by two restores Q16 pixel units.
     */
    result.x = sgl_fixed_div_pow2_i64(
        ((sgl_q16_ext_t)context->cos_angle * rel_x2) +
        ((sgl_q16_ext_t)context->sin_angle * rel_y2) +
        src_center_x2,
        1U);
    result.y = sgl_fixed_div_pow2_i64(
        (-(sgl_q16_ext_t)context->sin_angle * rel_x2) +
        ((sgl_q16_ext_t)context->cos_angle * rel_y2) +
        src_center_y2,
        1U);

    return result;
}

static SGL_ALWAYS_INLINE void sgl_rotate_copy_pixel(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp)
{
    sgl_int32_t channel;

    switch (bpp) {
    case SGL_BPP32:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        break;
    case SGL_BPP24:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        break;
    case SGL_BPP16:
        dst[0] = src[0];
        dst[1] = src[1];
        break;
    case SGL_BPP8:
        dst[0] = src[0];
        break;
    default:
        for (channel = 0; channel < bpp; ++channel) {
            dst[channel] = src[channel];
        }
        break;
    }
}

static SGL_ALWAYS_INLINE void sgl_rotate_clear_pixel(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp)
{
    sgl_int32_t channel;

    switch (bpp) {
    case SGL_BPP32:
        dst[0] = 0U;
        dst[1] = 0U;
        dst[2] = 0U;
        dst[3] = 0U;
        break;
    case SGL_BPP24:
        dst[0] = 0U;
        dst[1] = 0U;
        dst[2] = 0U;
        break;
    case SGL_BPP16:
        dst[0] = 0U;
        dst[1] = 0U;
        break;
    case SGL_BPP8:
        dst[0] = 0U;
        break;
    default:
        for (channel = 0; channel < bpp; ++channel) {
            dst[channel] = 0U;
        }
        break;
    }
}

static sgl_int64_t sgl_rotate_floor_div_positive(
    sgl_int64_t numerator,
    sgl_int64_t denominator)
{
    sgl_uint64_t magnitude;
    sgl_int64_t result;

    if (numerator < 0) {
        magnitude = (sgl_uint64_t)(-(numerator + 1)) + 1U;
        result = -(sgl_int64_t)(
            (magnitude + (sgl_uint64_t)denominator - 1U) /
            (sgl_uint64_t)denominator);
    }
    else {
        result = numerator / denominator;
    }

    return result;
}

static sgl_int64_t sgl_rotate_ceil_div_positive(
    sgl_int64_t numerator,
    sgl_int64_t denominator)
{
    sgl_uint64_t magnitude;
    sgl_int64_t result;

    if (numerator > 0) {
        result = (numerator + denominator - 1) / denominator;
    }
    else {
        magnitude = (sgl_uint64_t)(-(numerator + 1)) + 1U;
        result = -(sgl_int64_t)(
            magnitude / (sgl_uint64_t)denominator);
    }

    return result;
}

static void sgl_rotate_clip_valid_range(
    sgl_q16_ext_t origin,
    sgl_q16_t step,
    sgl_q16_ext_t minimum,
    sgl_q16_ext_t maximum,
    sgl_int64_t *start,
    sgl_int64_t *end)
{
    sgl_int64_t candidate_start = 0;
    sgl_int64_t candidate_end = -1;

    if (step > 0) {
        candidate_start = sgl_rotate_ceil_div_positive(
            minimum - origin, (sgl_int64_t)step);
        candidate_end = sgl_rotate_floor_div_positive(
            maximum - origin, (sgl_int64_t)step);
    }
    else if (step < 0) {
        sgl_int64_t positive_step;

        positive_step = -(sgl_int64_t)step;
        candidate_start = sgl_rotate_ceil_div_positive(
            origin - maximum, positive_step);
        candidate_end = sgl_rotate_floor_div_positive(
            origin - minimum, positive_step);
    }
    else if ((origin >= minimum) && (origin <= maximum)) {
        candidate_start = *start;
        candidate_end = *end;
    }
    else {
        candidate_start = 1;
        candidate_end = 0;
    }

    if (candidate_start > *start) {
        *start = candidate_start;
    }
    if (candidate_end < *end) {
        *end = candidate_end;
    }
}

static sgl_rotate_valid_range_t sgl_rotate_get_valid_range(
    const sgl_rotate_context_t *context,
    sgl_rotate_row_start_t position)
{
    sgl_rotate_valid_range_t result;
    sgl_int64_t start = 0;
    sgl_int64_t end = (sgl_int64_t)context->dst_width - 1;
    sgl_q16_ext_t minimum;
    sgl_q16_ext_t maximum;

    /* Nearest rounding accepts Q16 coordinates strictly above -0.5. */
    minimum = -(sgl_q16_ext_t)SGL_Q16_HALF + 1;
    maximum = ((sgl_q16_ext_t)context->src_width * SGL_Q16_ONE) -
        (sgl_q16_ext_t)SGL_Q16_HALF - 1;
    sgl_rotate_clip_valid_range(
        position.x, context->cos_angle, minimum, maximum, &start, &end);
    maximum = ((sgl_q16_ext_t)context->src_height * SGL_Q16_ONE) -
        (sgl_q16_ext_t)SGL_Q16_HALF - 1;
    sgl_rotate_clip_valid_range(
        position.y, -context->sin_angle, minimum, maximum, &start, &end);

    if (start <= end) {
        result.start = (sgl_int32_t)start;
        result.end = (sgl_int32_t)(end + 1);
    }
    else {
        result.start = 0;
        result.end = 0;
    }

    return result;
}

static SGL_ALWAYS_INLINE sgl_int32_t sgl_rotate_get_interior_offset(
    const sgl_rotate_context_t *context,
    sgl_q16_ext_t source_x,
    sgl_q16_ext_t source_y)
{
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t result;

    x = sgl_q16_round_to_int32(source_x);
    y = sgl_q16_round_to_int32(source_y);
    result = (sgl_int32_t)(
        ((sgl_size_t)(sgl_uint32_t)y *
         (sgl_size_t)(sgl_uint32_t)context->src_width) +
        (sgl_size_t)(sgl_uint32_t)x);

    return result;
}

static void sgl_rotate_clear_pixels(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t bpp,
    sgl_int32_t count)
{
    sgl_int32_t pixel;

    for (pixel = 0; pixel < count; ++pixel) {
        sgl_rotate_clear_pixel(dst, bpp);
        dst = &dst[bpp];
    }
}

static void sgl_rotate_run_mapped_row(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t dst_y)
{
    sgl_rotate_row_start_t position;
    sgl_rotate_valid_range_t valid;
    sgl_int32_t dst_x;
    sgl_uint8_t *dst_pixel;

    /* Compute one row origin, then advance by the inverse rotation matrix. */
    position = sgl_rotate_get_row_start(context, dst_y);
    valid = sgl_rotate_get_valid_range(context, position);
    dst_pixel = &dst[
        (sgl_size_t)(sgl_uint32_t)dst_y *
        (sgl_size_t)(sgl_uint32_t)context->dst_width *
        (sgl_size_t)(sgl_uint32_t)context->bpp];

    sgl_rotate_clear_pixels(dst_pixel, context->bpp, valid.start);
    dst_pixel = &dst_pixel[
        (sgl_size_t)(sgl_uint32_t)valid.start *
        (sgl_size_t)(sgl_uint32_t)context->bpp];
    position.x += (sgl_q16_ext_t)context->cos_angle * valid.start;
    position.y -= (sgl_q16_ext_t)context->sin_angle * valid.start;
    for (dst_x = valid.start; dst_x < valid.end; ++dst_x) {
        sgl_int32_t src_pixel;
        sgl_size_t src_offset;

        /* The row interval proves these rounded coordinates are in bounds. */
        src_pixel = sgl_rotate_get_interior_offset(
            context, position.x, position.y);
        src_offset = (sgl_size_t)(sgl_uint32_t)src_pixel *
            (sgl_size_t)(sgl_uint32_t)context->bpp;
        sgl_rotate_copy_pixel(
            &src[src_offset], dst_pixel, context->bpp);

        position.x += (sgl_q16_ext_t)context->cos_angle;
        position.y -= (sgl_q16_ext_t)context->sin_angle;
        dst_pixel = &dst_pixel[context->bpp];
    }
    sgl_rotate_clear_pixels(
        dst_pixel,
        context->bpp,
        context->dst_width - valid.end);
}

static void sgl_rotate_run_lookup_row(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *lut,
    sgl_int32_t dst_y)
{
    sgl_int32_t dst_x;
    sgl_int32_t valid_start;
    sgl_int32_t valid_end;
    sgl_int32_t src_pixel;
    sgl_size_t dst_pixel;
    sgl_uint8_t *dst_bytes;

    /* A rotated rectangle intersects each output row in one valid interval. */
    dst_pixel = (sgl_size_t)(sgl_uint32_t)dst_y *
        (sgl_size_t)(sgl_uint32_t)context->dst_width;
    valid_start = lut->row_ranges[
        (sgl_size_t)(sgl_uint32_t)dst_y << 1U];
    valid_end = lut->row_ranges[
        ((sgl_size_t)(sgl_uint32_t)dst_y << 1U) + 1U];
    dst_bytes = &dst[
        dst_pixel * (sgl_size_t)(sgl_uint32_t)context->bpp];
    for (dst_x = 0; dst_x < valid_start; ++dst_x) {
        sgl_rotate_clear_pixel(dst_bytes, context->bpp);
        ++dst_pixel;
        dst_bytes = &dst_bytes[context->bpp];
    }
    for (dst_x = valid_start; dst_x < valid_end; ++dst_x) {
        src_pixel = lut->source_offsets[dst_pixel];
        sgl_rotate_copy_pixel(
            &src[(sgl_size_t)(sgl_uint32_t)src_pixel *
                 (sgl_size_t)(sgl_uint32_t)context->bpp],
            dst_bytes,
            context->bpp);
        ++dst_pixel;
        dst_bytes = &dst_bytes[context->bpp];
    }
    for (dst_x = valid_end; dst_x < context->dst_width; ++dst_x) {
        sgl_rotate_clear_pixel(dst_bytes, context->bpp);
        ++dst_pixel;
        dst_bytes = &dst_bytes[context->bpp];
    }
}

sgl_result_t sgl_rotate_context_initialize(
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t bpp,
    sgl_float64_t angle,
    sgl_rotate_context_t *context)
{
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    if ((context != SGL_NULL) &&
        (src_width > 0) &&
        (src_height > 0) &&
        (bpp > 0) &&
        (sgl_rotate_angle_is_valid(angle) == SGL_TRUE)) {
        /* Build immutable geometry once so row workers only map and copy. */
        context->src_width = src_width;
        context->src_height = src_height;
        context->bpp = bpp;
        context->normalized_angle = sgl_rotate_angle_to_q16(angle);
        sgl_rotate_get_trig(
            context->normalized_angle,
            &context->sin_angle,
            &context->cos_angle);
        sgl_rotate_calculate_size(context);
        if (sgl_rotate_calculate_bytes(context) == SGL_TRUE) {
            result = SGL_SUCCESS;
        }
    }

    return result;
}

sgl_result_t sgl_rotate_validate_buffers(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((src == SGL_NULL) || (dst == SGL_NULL)) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

sgl_result_t sgl_rotate_validate_lut(
    const sgl_rotate_context_t *context,
    const sgl_rotate_lookup_t *lut)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((lut != SGL_NULL) &&
        ((lut->src_width != context->src_width) ||
         (lut->src_height != context->src_height) ||
         (lut->dst_width != context->dst_width) ||
         (lut->dst_height != context->dst_height) ||
         (lut->normalized_angle != context->normalized_angle) ||
         (lut->source_offsets == SGL_NULL) ||
         (lut->row_ranges == SGL_NULL))) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

static void sgl_rotate_run_range(
    const sgl_parallel_range_t *range,
    const sgl_rotate_data_t *data)
{
    sgl_int32_t row;
    sgl_int32_t end;

    end = range->start + range->count;
    /* Select the LUT path once per worker range rather than once per pixel. */
    if (data->lut != SGL_NULL) {
        for (row = range->start; row < end; ++row) {
            sgl_rotate_run_lookup_row(
                data->context,
                data->src,
                data->dst,
                data->lut,
                row);
        }
    }
    else {
        for (row = range->start; row < end; ++row) {
            sgl_rotate_run_mapped_row(
                data->context,
                data->src,
                data->dst,
                row);
        }
    }
}

/* cppcheck-suppress constParameterCallback */
static void sgl_rotate_routine(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie)
{
    const sgl_parallel_range_t *range;
    const sgl_rotate_data_t *data;

    range = sgl_parallel_range_from_memory(current);
    /* SGL-MEM-DEV-001: typed conversion from threadpool cookie storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    data = (const sgl_rotate_data_t *)cookie;
    sgl_rotate_run_range(range, data);
}

static sgl_bool_t sgl_rotate_should_parallel(
    const sgl_threadpool_t *pool,
    const sgl_rotate_context_t *context)
{
    sgl_bool_t result = SGL_FALSE;

#if defined(SGL_CFG_HAS_THREAD)
    /* Row workers amortize thread setup once the output reaches 64K pixels. */
    if ((pool != SGL_NULL) &&
        (sgl_threadpool_get_num_threads(pool) > 1U) &&
        (((sgl_uint64_t)(sgl_uint32_t)context->dst_width *
          (sgl_uint64_t)(sgl_uint32_t)context->dst_height) >=
         SGL_ROTATE_MIN_PARALLEL_PIXELS)) {
        result = SGL_TRUE;
    }
#else
    (void)pool;
    (void)context;
#endif  /* SGL_CFG_HAS_THREAD */

    return result;
}

static sgl_rotate_data_t sgl_rotate_make_data(
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut)
{
    sgl_rotate_data_t result;

    result.context = context;
    result.src = src;
    result.dst = dst;
    result.lut = lut;

    return result;
}

sgl_result_t sgl_rotate_run_scalar(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_rotate_context_t *context,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    const sgl_rotate_lookup_t *SGL_RESTRICT lut)
{
    sgl_rotate_data_t data;
    sgl_parallel_range_t range;
    sgl_result_t result = SGL_SUCCESS;

    data = sgl_rotate_make_data(context, src, dst, lut);
    if (sgl_rotate_should_parallel(pool, context) == SGL_TRUE) {
        result = sgl_parallel_for(
            pool,
            context->dst_height,
            1,
            sgl_rotate_routine,
            (void *)&data);
    }
    else {
        range.start = 0;
        range.count = context->dst_height;
        sgl_rotate_run_range(&range, &data);
    }

    return result;
}
