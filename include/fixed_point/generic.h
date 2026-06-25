#ifndef SGL_FIXED_POINT_GENERIC_H_
#define SGL_FIXED_POINT_GENERIC_H_

typedef int16_t                     sgl_q11_t;
typedef int32_t                     sgl_q11_ext_t;

enum {
    SGL_Q11_FRAC_BITS = 11,
    SGL_Q11_ONE = 2048,
    SGL_Q11_HALF = 1024
};

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_int_to_q11(sgl_q11_ext_t value)
{
    return value * SGL_Q11_ONE;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_get_int_part(sgl_q11_ext_t value)
{
    return value / SGL_Q11_ONE;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_get_frac_part(sgl_q11_ext_t value)
{
    return value % SGL_Q11_ONE;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_round_up(sgl_q11_ext_t value)
{
    return value + SGL_Q11_HALF;
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_shift_down(sgl_q11_ext_t value)
{
    sgl_q11_ext_t result;

    if (value >= 0) {
        result = value / SGL_Q11_ONE;
    }
    else {
        result = -((-value + (SGL_Q11_ONE - 1)) / SGL_Q11_ONE);
    }

    return result;
}

static SGL_ALWAYS_INLINE sgl_q11_t sgl_q11_mul(sgl_q11_t a, sgl_q11_t b)
{
    sgl_q11_ext_t prod = (sgl_q11_ext_t)a * (sgl_q11_ext_t)b;   /* Q22 = Q11 x Q11 */
    prod = sgl_q11_round_up(prod);                               /* Rounding */
    return (sgl_q11_t)sgl_q11_shift_down(prod);                  /* Q11 */
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_q11_ext_mul(sgl_q11_ext_t a, sgl_q11_ext_t b)
{
    sgl_q11_ext_t prod = a * b;     /* Q22 = Q11 x Q11 */
    prod = sgl_q11_round_up(prod);   /* Rounding */
    return sgl_q11_shift_down(prod); /* Q11 */
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
