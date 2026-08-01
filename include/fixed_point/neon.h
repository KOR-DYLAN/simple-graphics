/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_FIXED_POINT_NEON_H_
#define SGL_FIXED_POINT_NEON_H_

#include <arm_neon.h>

/** @addtogroup sgl_fixed_point
 *  @{
 */
/** Eight signed 16-bit Q11 lanes. */
typedef int16x8_t   sgl_simd_q11_t;
/** Four signed 32-bit extended Q11 lanes. */
typedef int32x4_t   sgl_simd_q11_ext_t;

/**
 * @brief Multiplies eight Q11 lane pairs with rounding and saturation.
 * @param[in] a First Q11 vector.
 * @param[in] b Second Q11 vector.
 * @return Q11 vector equivalent to `(a * b) >> 11` per lane.
 */
static SGL_ALWAYS_INLINE sgl_simd_q11_t sgl_simd_q11_mul(
    sgl_simd_q11_t a,
    sgl_simd_q11_t b)
{
    int32x4_t prod_lo;
    int32x4_t prod_hi;
    sgl_simd_q11_t result;

    prod_lo = vmull_s16(vget_low_s16(a), vget_low_s16(b));
    prod_hi = vmull_s16(vget_high_s16(a), vget_high_s16(b));

    /*
     * Rounding shift right removes eleven Q22 fractional bits and restores
     * the Q11 scale.
     */
    prod_lo = vrshrq_n_s32(prod_lo, SGL_Q11_FRAC_BITS);
    prod_hi = vrshrq_n_s32(prod_hi, SGL_Q11_FRAC_BITS);

    result = vcombine_s16(vqmovn_s32(prod_lo), vqmovn_s32(prod_hi));

    return result;
}

/**
 * @brief Multiplies four extended Q11 lane pairs.
 * @param[in] a First extended Q11 vector.
 * @param[in] b Second extended Q11 vector.
 * @return Rounded and saturated extended Q11 products.
 *
 * Each 32-bit lane stores one Q11 value. Multiplication creates a Q22
 * intermediate, then a rounding right shift restores the Q11 scale.
 */
static SGL_ALWAYS_INLINE sgl_simd_q11_ext_t sgl_simd_q11_ext_mul(
    sgl_simd_q11_ext_t a,
    sgl_simd_q11_ext_t b)
{
    int64x2_t prod0;
    int64x2_t prod1;
    sgl_simd_q11_ext_t result;

    prod0 = vmull_s32(vget_low_s32(a), vget_low_s32(b));
    prod1 = vmull_s32(vget_high_s32(a), vget_high_s32(b));
    /*
     * Rounding shift right removes eleven Q22 fractional bits before the
     * values are narrowed back to 32-bit Q11 lanes.
     */
    result = vcombine_s32(
        vqrshrn_n_s64(prod0, SGL_Q11_FRAC_BITS),
        vqrshrn_n_s64(prod1, SGL_Q11_FRAC_BITS));

    return result;
}

/**
 * @brief Saturates eight signed 32-bit channel lanes to unsigned bytes.
 * @param[in] lo First four signed channel values.
 * @param[in] hi Last four signed channel values.
 * @return Eight values saturated to the range 0 through 255.
 */
static SGL_ALWAYS_INLINE uint8x8_t sgl_simd_clamp_u8_i32(
    int32x4_t lo,
    int32x4_t hi)
{
    uint16x4_t u16_lo;
    uint16x4_t u16_hi;
    uint8x8_t result;

    u16_lo = vqmovun_s32(lo);
    u16_hi = vqmovun_s32(hi);
    result = vqmovn_u16(vcombine_u16(u16_lo, u16_hi));

    return result;
}

/** @} */

#endif  /* !SGL_FIXED_POINT_NEON_H_ */
