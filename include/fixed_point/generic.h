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

/** @defgroup sgl_fixed_point Fixed-point arithmetic
 *  @brief Saturating scalar helpers for Q11 and Q16 values.
 *  @{
 */
/** Signed Q11 value stored in 16 bits. */
typedef sgl_int16_t     sgl_q11_t;
/** Extended signed Q11 value stored in 32 bits. */
typedef sgl_int32_t     sgl_q11_ext_t;
/** Signed Q16 value stored in 32 bits. */
typedef sgl_int32_t     sgl_q16_t;
/** Extended signed Q16 value stored in 64 bits. */
typedef sgl_int64_t     sgl_q16_ext_t;

/** Number of fractional bits in a Q11 value. */
#define SGL_Q11_FRAC_BITS    (11U)
/** Q11 representation of 1.0 (`1 << 11`). */
#define SGL_Q11_ONE          (2048)
/** Q11 representation of 0.5. */
#define SGL_Q11_HALF         (1024)
/** Mask selecting all eleven Q11 fractional bits. */
#define SGL_Q11_MASK         (2047U)
/** Number of fractional bits in a Q16 value. */
#define SGL_Q16_FRAC_BITS    (16U)
/** Q16 representation of 1.0 (`1 << 16`). */
#define SGL_Q16_ONE          (65536)
/** Q16 representation of 0.5. */
#define SGL_Q16_HALF         (32768)
/** Mask selecting all sixteen Q16 fractional bits. */
#define SGL_Q16_MASK         (65535U)
/** Bit width used by the portable signed-division helpers. */
#define SGL_FIXED_I64_BITS   (64U)
/** Maximum signed 32-bit value without standard-library constants. */
#define SGL_FIXED_INT32_MAX  (2147483647)
/** Minimum signed 32-bit value without standard-library constants. */
#define SGL_FIXED_INT32_MIN  (-2147483647 - 1)
/** Maximum signed 16-bit value without standard-library constants. */
#define SGL_FIXED_INT16_MAX  (32767)
/** Minimum signed 16-bit value without standard-library constants. */
#define SGL_FIXED_INT16_MIN  (-32767 - 1)

/**
 * @brief Saturates a signed 64-bit value to the signed 32-bit range.
 * @param[in] value Value to clamp.
 * @return Saturated signed 32-bit value.
 */
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

/**
 * @brief Saturates a signed 32-bit value to the signed 16-bit range.
 * @param[in] value Value to clamp.
 * @return Saturated signed 16-bit value.
 */
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

/**
 * @brief Divides by a power of two with C99 signed-division semantics.
 * @param[in] value Dividend.
 * @param[in] shift Base-two exponent in the range 0 through 63.
 * @return `value / (1 << shift)`, truncated toward zero, or 0 when @p shift
 *         is outside the supported range.
 *
 * The shift is applied only to an unsigned magnitude, avoiding an
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

/**
 * @brief Divides by a power of two while rounding toward negative infinity.
 * @param[in] value Dividend.
 * @param[in] shift Base-two exponent.
 * @return Portable equivalent of an arithmetic signed right shift.
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

/**
 * @brief Converts an integer to extended Q11 with saturation.
 * @param[in] value Integer value.
 * @return Q11 value equal to `value * 2048`, saturated to 32 bits.
 */
static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_int_to_q11(sgl_q11_ext_t value)
{
    sgl_int64_t scaled;
    sgl_q11_ext_t result;

    scaled = (sgl_int64_t)value * (sgl_int64_t)SGL_Q11_ONE;
    result = sgl_fixed_clamp_i64_to_i32(scaled);

    return result;
}

/**
 * @brief Extracts the integer part of an extended Q11 value.
 * @param[in] value Extended Q11 value.
 * @return Integer part truncated toward zero.
 */
static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_get_int_part(sgl_q11_ext_t value)
{
    sgl_q11_ext_t result;

    /* Q11 -> integer: remove eleven fractional bits. */
    result = (sgl_q11_ext_t)sgl_fixed_div_pow2_i64(
        (sgl_int64_t)value, SGL_Q11_FRAC_BITS);

    return result;
}

/**
 * @brief Extracts the signed fractional part of an extended Q11 value.
 * @param[in] value Extended Q11 value.
 * @return Fractional numerator in the range -2047 through 2047.
 */
static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_get_frac_part(sgl_q11_ext_t value)
{
    sgl_q11_ext_t integer_part;
    sgl_q11_ext_t result;

    integer_part = sgl_q11_get_int_part(value);
    result = value - (integer_part * SGL_Q11_ONE);

    return result;
}

/**
 * @brief Adds the Q11 half-unit used before rounded conversion.
 * @param[in] value Extended Q11 value.
 * @return `value + 1024`, saturated to signed 32 bits.
 */
static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_round_up(sgl_q11_ext_t value)
{
    sgl_int64_t rounded;
    sgl_q11_ext_t result;

    rounded = (sgl_int64_t)value + (sgl_int64_t)SGL_Q11_HALF;
    result = sgl_fixed_clamp_i64_to_i32(rounded);

    return result;
}

/**
 * @brief Removes Q11 fractional bits while rounding toward negative infinity.
 * @param[in] value Extended Q11 value.
 * @return Integer result of a portable arithmetic 11-bit right shift.
 */
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

/**
 * @brief Multiplies two Q11 values with rounding and saturation.
 * @param[in] a First Q11 factor.
 * @param[in] b Second Q11 factor.
 * @return Saturated Q11 product.
 */
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

/**
 * @brief Multiplies two extended Q11 values with rounding and saturation.
 * @param[in] a First Q11 factor.
 * @param[in] b Second Q11 factor.
 * @return Saturated extended Q11 product.
 */
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

/**
 * @brief Converts a signed 32-bit integer to extended Q16.
 * @param[in] value Integer value.
 * @return Exact 64-bit Q16 representation.
 */
static SGL_ALWAYS_INLINE sgl_q16_ext_t sgl_int_to_q16(sgl_int32_t value)
{
    sgl_q16_ext_t result;

    result = (sgl_q16_ext_t)value * (sgl_q16_ext_t)SGL_Q16_ONE;

    return result;
}

/**
 * @brief Rounds an extended Q16 value to the nearest signed 32-bit integer.
 * @param[in] value Extended Q16 value.
 * @return Rounded and saturated integer.
 */
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

/**
 * @brief Saturates a signed 32-bit channel value to 0 through 255.
 * @param[in] val Channel value.
 * @return Saturated unsigned byte.
 */
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

/** @} */

#endif  /* !SGL_FIXED_POINT_GENERIC_H_ */
