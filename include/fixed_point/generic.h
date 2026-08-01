/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_FIXED_POINT_GENERIC_H_
#define SGL_FIXED_POINT_GENERIC_H_

typedef sgl_int16_t     sgl_q11_t;
typedef sgl_int32_t     sgl_q11_ext_t;
typedef sgl_int32_t     sgl_q16_t;
typedef sgl_int64_t     sgl_q16_ext_t;

#define SGL_Q11_FRAC_BITS    (11U)
/* 2048 is 1U << 11 and represents 1.0 in Q11. */
#define SGL_Q11_ONE          (2048)
/* 1024 is one half of the Q11 unit. */
#define SGL_Q11_HALF         (1024)
/* 2047 masks the eleven Q11 fractional bits. */
#define SGL_Q11_MASK         (2047U)
#define SGL_Q16_FRAC_BITS    (16U)
/* 65536 is 1U << 16 and represents 1.0 in Q16. */
#define SGL_Q16_ONE          (65536)
/* 32768 is one half of the Q16 unit. */
#define SGL_Q16_HALF         (32768)
/* 65535 masks the sixteen Q16 fractional bits. */
#define SGL_Q16_MASK         (65535U)
#define SGL_FIXED_I64_BITS   (64U)
#define SGL_FIXED_INT32_MAX  (2147483647)
#define SGL_FIXED_INT32_MIN  (-2147483647 - 1)
#define SGL_FIXED_INT16_MAX  (32767)
#define SGL_FIXED_INT16_MIN  (-32767 - 1)

static SGL_ALWAYS_INLINE sgl_int32_t sgl_fixed_clamp_i64_to_i32(
    sgl_int64_t value)
{
    sgl_int32_t result;

    if (value > (sgl_int64_t)SGL_FIXED_INT32_MAX) {
        result = SGL_FIXED_INT32_MAX;
    }
    else if (value < SGL_FIXED_INT32_MIN) {
        result = SGL_FIXED_INT32_MIN;
    }
    else {
        result = (sgl_int32_t)value;
    }

    return result;
}

static SGL_ALWAYS_INLINE sgl_int16_t sgl_fixed_clamp_i32_to_i16(
    sgl_int32_t value)
{
    sgl_int16_t result;

    if (value > SGL_FIXED_INT16_MAX) {
        result = SGL_FIXED_INT16_MAX;
    }
    else if (value < SGL_FIXED_INT16_MIN) {
        result = SGL_FIXED_INT16_MIN;
    }
    else {
        result = (sgl_int16_t)value;
    }

    return result;
}

/*
 * Divide by 2^shift with C99 signed-division semantics.
 * The shift is applied only to an unsigned magnitude, avoiding the
 * implementation-defined right shift of a negative signed integer.
 */
static SGL_ALWAYS_INLINE sgl_int64_t sgl_fixed_div_pow2_i64(
    sgl_int64_t value,
    sgl_uint32_t shift)
{
    sgl_int64_t result = 0;

    if (shift < SGL_FIXED_I64_BITS) {
        sgl_uint64_t magnitude;
        sgl_uint64_t shifted;

        if (value < 0) {
            magnitude = (sgl_uint64_t)(-(value + 1)) + 1U;
            shifted = magnitude >> shift;
            result = -(sgl_int64_t)shifted;
        }
        else {
            magnitude = (sgl_uint64_t)value;
            shifted = magnitude >> shift;
            result = (sgl_int64_t)shifted;
        }
    }

    return result;
}

/*
 * Divide by 2^shift while rounding toward negative infinity.
 * This is the portable equivalent of an arithmetic signed right shift.
 */
static SGL_ALWAYS_INLINE sgl_int64_t sgl_fixed_floor_pow2_i64(
    sgl_int64_t value,
    sgl_uint32_t shift)
{
    sgl_int64_t result = 0;

    if (shift < SGL_FIXED_I64_BITS) {
        sgl_uint64_t magnitude;
        sgl_uint64_t shifted;

        if (value < 0) {
            magnitude = (sgl_uint64_t)(-(value + 1)) + 1U;
            /* The low-bit mask rounds any negative remainder toward -infinity. */
            magnitude += (((sgl_uint64_t)1U << shift) - 1U);
            shifted = magnitude >> shift;
            result = -(sgl_int64_t)shifted;
        }
        else {
            magnitude = (sgl_uint64_t)value;
            shifted = magnitude >> shift;
            result = (sgl_int64_t)shifted;
        }
    }
    else if (value < 0) {
        result = -1;
    }
    else {
        result = 0;
    }

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_int_to_q11(sgl_q11_ext_t value)
{
    sgl_int64_t scaled;
    sgl_q11_ext_t result;

    scaled = (sgl_int64_t)value * (sgl_int64_t)SGL_Q11_ONE;
    result = sgl_fixed_clamp_i64_to_i32(scaled);

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_get_int_part(sgl_q11_ext_t value)
{
    sgl_q11_ext_t result;

    /* Q11 -> integer: remove eleven fractional bits. */
    result = (sgl_q11_ext_t)sgl_fixed_div_pow2_i64(
        (sgl_int64_t)value, SGL_Q11_FRAC_BITS);

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_get_frac_part(sgl_q11_ext_t value)
{
    sgl_q11_ext_t integer_part;
    sgl_q11_ext_t result;

    integer_part = sgl_q11_get_int_part(value);
    result = value - (integer_part * SGL_Q11_ONE);

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_round_up(sgl_q11_ext_t value)
{
    sgl_int64_t rounded;
    sgl_q11_ext_t result;

    rounded = (sgl_int64_t)value + (sgl_int64_t)SGL_Q11_HALF;
    result = sgl_fixed_clamp_i64_to_i32(rounded);

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_shift_down(sgl_q11_ext_t value)
{
    sgl_q11_ext_t result;

    if (value >= 0) {
        sgl_uint32_t shifted;

        /* Q11 -> integer: remove eleven fractional bits. */
        shifted = (sgl_uint32_t)value >> SGL_Q11_FRAC_BITS;
        result = (sgl_q11_ext_t)shifted;
    }
    else {
        sgl_uint32_t magnitude;

        magnitude = (sgl_uint32_t)(-(value + 1)) + 1U;
        /* Add a Q11 mask before shifting to implement floor for negatives. */
        magnitude = (magnitude + SGL_Q11_MASK) >> SGL_Q11_FRAC_BITS;
        result = -(sgl_q11_ext_t)magnitude;
    }

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_t sgl_q11_mul(sgl_q11_t a, sgl_q11_t b)
{
    sgl_q11_ext_t prod;
    sgl_q11_ext_t shifted;
    sgl_q11_t result;

    prod = (sgl_q11_ext_t)a * (sgl_q11_ext_t)b;  /* Q22 = Q11 x Q11. */
    prod = sgl_q11_round_up(prod);
    /* Remove eleven fractional product bits to restore Q11. */
    shifted = sgl_q11_shift_down(prod);
    result = sgl_fixed_clamp_i32_to_i16(shifted);

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_ext_mul(sgl_q11_ext_t a, sgl_q11_ext_t b)
{
    sgl_int64_t prod;
    sgl_int64_t shifted;
    sgl_q11_ext_t result;

    prod = (sgl_int64_t)a * (sgl_int64_t)b;  /* Q22 = Q11 x Q11. */
    prod += (sgl_int64_t)SGL_Q11_HALF;
    /* Remove eleven fractional product bits to restore Q11. */
    shifted = sgl_fixed_floor_pow2_i64(prod, SGL_Q11_FRAC_BITS);
    result = sgl_fixed_clamp_i64_to_i32(shifted);

    return result;
}

static SGL_ALWAYS_INLINE sgl_q16_ext_t sgl_int_to_q16(sgl_int32_t value)
{
    sgl_q16_ext_t result;

    result = (sgl_q16_ext_t)value * (sgl_q16_ext_t)SGL_Q16_ONE;

    return result;
}

static SGL_ALWAYS_INLINE sgl_int32_t sgl_q16_round_to_int32(
    sgl_q16_ext_t value)
{
    sgl_uint64_t magnitude;
    sgl_int32_t result;

    if (value < 0) {
        magnitude = (sgl_uint64_t)(-(value + 1)) + 1U;
        /* Add half, then remove sixteen fractional bits for nearest rounding. */
        magnitude += (sgl_uint64_t)SGL_Q16_HALF;
        magnitude >>= SGL_Q16_FRAC_BITS;
        if (magnitude >= ((sgl_uint64_t)SGL_FIXED_INT32_MAX + 1U)) {
            result = SGL_FIXED_INT32_MIN;
        }
        else {
            result = -(sgl_int32_t)magnitude;
        }
    }
    else {
        magnitude = (sgl_uint64_t)value;
        /* Add half, then remove sixteen fractional bits for nearest rounding. */
        magnitude += (sgl_uint64_t)SGL_Q16_HALF;
        magnitude >>= SGL_Q16_FRAC_BITS;
        if (magnitude > (sgl_uint64_t)SGL_FIXED_INT32_MAX) {
            result = SGL_FIXED_INT32_MAX;
        }
        else {
            result = (sgl_int32_t)magnitude;
        }
    }

    return result;
}

static SGL_ALWAYS_INLINE sgl_uint8_t sgl_clamp_u8_i32(sgl_int32_t val)
{
    sgl_uint8_t u8_val = (sgl_uint8_t)val;

    /* Any set bit outside the low eight bits means val is outside 0..255. */
    if ((val & ~0xFF) != 0) {
        if (val < 0) {
            u8_val = 0U;
        }
        else {
            u8_val = 255U;
        }
    }

    return u8_val;
}

#endif  /* !SGL_FIXED_POINT_GENERIC_H_ */
