#ifndef SGL_FIXED_POINT__H__
#define SGL_FIXED_POINT__H__

#include <stdint.h>
#include <arm_neon.h>
#include <sgl-compiler.h>

#define SGL_BIT(n)                  (1 << (n))
#define SGL_Q15_FRAC_BITS           (15)
#define SGL_Q15_ONE                 (1 << SGL_Q15_FRAC_BITS)
#define SGL_Q15_MASK                (SGL_Q15_ONE - 1)

#define SGL_INT_TO_Q15(num)         ((sgl_q15_t)(num) << SGL_Q15_FRAC_BITS)
#define SGL_Q15_GET_INT_PART(num)   ((num) >> SGL_Q15_FRAC_BITS)
#define SGL_Q15_GET_FRAC_PART(num)  ((num) & SGL_Q15_MASK)
#define SGL_Q15_ROUNDUP(num)        ((num) + (typeof((num)))(1U << (SGL_Q15_FRAC_BITS - 1)))
#define SGL_Q15_SHIFTDOWN(num)      ((num) >> SGL_Q15_FRAC_BITS)

#define SGL_SIMD_Q15_ROUNDUP(num)   vaddq_u32((num), vdupq_n_u32(1U << (SGL_Q15_FRAC_BITS - 1)))
#define SGL_SIMD_Q15_SHIFTDOWN(num) vreinterpretq_s32_u32(vshrq_n_u32((num), SGL_Q15_FRAC_BITS))

typedef int32_t                     sgl_q15_t;
typedef int32x4_t                   sgl_simd_q15_t;

static ALWAYS_INLINE sgl_q15_t sgl_q15_mul(sgl_q15_t a, sgl_q15_t b) {
    /* Avoid UB(Undefined Behavior) by changing 'signed' to 'unsigned'. */
    uint32_t ua = (uint32_t)a;  /* Q15 */
    uint32_t ub = (uint32_t)b;  /* Q15 */
    uint32_t prod = ua * ub;    /* Q30 = Q15 x Q15 */

    /* Rounding */
    prod = (uint32_t)SGL_Q15_ROUNDUP(prod);

    return (sgl_q15_t)SGL_Q15_SHIFTDOWN(prod);  /* Q15 */
}

static ALWAYS_INLINE uint8_t sgl_clamp_u8_i32(int32_t val)
{
    uint8_t u8_val = (uint8_t)val;

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

static ALWAYS_INLINE sgl_simd_q15_t sgl_simd_q15_mul(sgl_simd_q15_t a, sgl_q15_t b) {
    /* Avoid UB(Undefined Behavior) by changing 'signed' to 'unsigned'. */
    uint32x4_t ua = vreinterpretq_u32_s32(a);           /* Q15 */
    uint32_t ub = (uint32_t)b;                          /* Q15 */
    uint32x4_t prod = vmulq_u32(ua, vdupq_n_u32(ub));   /* Q30 = Q15 x Q15 */

    /* Rounding */
    prod = SGL_SIMD_Q15_ROUNDUP(prod);

    return SGL_SIMD_Q15_SHIFTDOWN(prod);  /* Q15 */
}

static ALWAYS_INLINE uint8x8_t sgl_simd_clamp_u8_i32(int32x4_t lo, int32x4_t hi)
{
    const int32x4_t zero = vdupq_n_s32(0);
    const int32x4_t max_u8 = vdupq_n_s32(255);
    int32x4_t clamped_lo, clamped_hi;
    uint16x4_t u16_lo, u16_hi;

    clamped_lo = vmaxq_s32(lo, zero);
    clamped_lo = vminq_s32(clamped_lo, max_u8);

    clamped_hi = vmaxq_s32(hi, zero);
    clamped_hi = vminq_s32(clamped_hi, max_u8);

    u16_lo = vmovn_u32(vreinterpretq_u32_s32(clamped_lo));
    u16_hi = vmovn_u32(vreinterpretq_u32_s32(clamped_hi));

    return vmovn_u16(vcombine_u16(u16_lo, u16_hi));
}

#endif  /* !SGL_FIXED_POINT__H__ */
