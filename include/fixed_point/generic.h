#ifndef SGL_FIXED_POINT_GENERIC_H_
#define SGL_FIXED_POINT_GENERIC_H_

#define SGL_BIT(n)                  (1 << (n))
#define SGL_Q11_FRAC_BITS           (11)
#define SGL_Q11_ONE                 (1 << SGL_Q11_FRAC_BITS)
#define SGL_Q11_MASK                (SGL_Q11_ONE - 1)

#define SGL_INT_TO_Q11(num)         ((num) << SGL_Q11_FRAC_BITS)
#define SGL_Q11_GET_INT_PART(num)   ((num) >> SGL_Q11_FRAC_BITS)
#define SGL_Q11_GET_FRAC_PART(num)  ((num) & SGL_Q11_MASK)
#define SGL_Q11_ROUNDUP(num)        ((num) + (SGL_TYPEOF((num)))(1 << (SGL_Q11_FRAC_BITS - 1)))
#define SGL_Q11_SHIFTDOWN(num)      ((num) >> SGL_Q11_FRAC_BITS)

typedef int16_t                     sgl_q11_t;
typedef int32_t                     sgl_q11_ext_t;

static SGL_ALWAYS_INLINE sgl_q11_t sgl_q11_mul(sgl_q11_t a, sgl_q11_t b)
{
    sgl_q11_ext_t prod = (sgl_q11_ext_t)a * (sgl_q11_ext_t)b;   /* Q22 = Q11 x Q11 */
    prod = SGL_Q11_ROUNDUP(prod);                               /* Rounding */
    return (sgl_q11_t)SGL_Q11_SHIFTDOWN(prod);                  /* Q11 */
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_ext_mul(sgl_q11_ext_t a, sgl_q11_ext_t b)
{
    sgl_q11_ext_t prod = a * b;     /* Q22 = Q11 x Q11 */
    prod = SGL_Q11_ROUNDUP(prod);   /* Rounding */
    return SGL_Q11_SHIFTDOWN(prod); /* Q11 */
}

static SGL_ALWAYS_INLINE uint8_t sgl_clamp_u8_i32(int32_t val)
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

#endif  /* !SGL_FIXED_POINT_GENERIC_H_ */
