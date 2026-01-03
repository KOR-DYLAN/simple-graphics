#ifndef SGL_FIXED_POINT_NEON_H_
#define SGL_FIXED_POINT_NEON_H_

#include <arm_neon.h>

typedef int16x8_t   sgl_simd_q11_t;
typedef int16x4_t   sgl_simd_q11_ext_t;

/**
 * Optimized Q11 Multiplication: (a * b) >> 11
 * Uses Rounding Shift Right for better precision and performance.
 */
static SGL_ALWAYS_INLINE sgl_simd_q11_t sgl_simd_q11_mul(sgl_simd_q11_t a, sgl_simd_q11_t b) {
    /* Step 1: Multiply Long (16-bit * 16-bit -> 32-bit) */
    int32x4_t prod_lo = vmull_s16(vget_low_s16(a), vget_low_s16(b));
    int32x4_t prod_hi = vmull_s16(vget_high_s16(a), vget_high_s16(b));

    /* Step 2: Rounding Shift Right (32-bit >> 11)
     * Effectively performs: (val + (1 << 10)) >> 11 in one instruction */
    prod_lo = vrshrq_n_s32(prod_lo, SGL_Q11_FRAC_BITS);
    prod_hi = vrshrq_n_s32(prod_hi, SGL_Q11_FRAC_BITS);

    /* Step 3: Narrow back to 16-bit with Saturation */
    return vcombine_s16(vqmovn_s32(prod_lo), vqmovn_s32(prod_hi));
}

/**
 * Optimized Clamp: int32x4_t (low/high) -> uint8x8_t
 * Performs saturating narrowing from 32-bit signed to 8-bit unsigned.
 */
static SGL_ALWAYS_INLINE uint8x8_t sgl_simd_clamp_u8_i32(int32x4_t lo, int32x4_t hi)
{
    /* Step 1: 32-bit Signed -> 16-bit Unsigned with Saturation 
     * Clamps values: < 0 to 0, > 65535 to 65535 */
    uint16x4_t u16_lo = vqmovun_s32(lo);
    uint16x4_t u16_hi = vqmovun_s32(hi);

    /* Step 2: 16-bit Unsigned -> 8-bit Unsigned with Saturation 
     * Clamps values: > 255 to 255 */
    return vqmovn_u16(vcombine_u16(u16_lo, u16_hi));
}

#endif  /* !SGL_FIXED_POINT_NEON_H_ */
