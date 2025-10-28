#ifndef SGL_FIXED_POINT__H__
#define SGL_FIXED_POINT__H__

#include <stdint.h>

#define SGL_BIT(n)                  (1 << (n))
#define SGL_Q15_FRAC_BITS           (15)
#define SGL_Q15_ONE                 (1 << SGL_Q15_FRAC_BITS)
#define SGL_Q15_MASK                (SGL_Q15_ONE - 1)

#define SGL_INT_TO_Q15(num)         ((sgl_q15_t)(num) << SGL_Q15_FRAC_BITS)
#define SGL_Q15_GET_INT_PART(num)   ((num) >> SGL_Q15_FRAC_BITS)
#define SGL_Q15_GET_FRAC_PART(num)  ((num) & SGL_Q15_MASK)
#define SGL_Q15_ROUNDUP(num)        ((num) + (typeof((num)))(1U << (SGL_Q15_FRAC_BITS - 1)))
#define SGL_Q15_SHIFTDOWN(num)      ((num) >> SGL_Q15_FRAC_BITS)

typedef int32_t                     sgl_q15_t;

static inline sgl_q15_t sgl_q15_mul(sgl_q15_t a, sgl_q15_t b) {
    /* Avoid UB(Undefined Behavior) by changing 'signed' to 'unsigned'. */
    uint32_t ua = (uint32_t)a;  /* Q15 */
    uint32_t ub = (uint32_t)b;  /* Q15 */
    uint32_t prod = ua * ub;    /* Q30 = Q15 x Q15 */

    /* Rounding */
    prod = (uint32_t)SGL_Q15_ROUNDUP(prod);

    return (sgl_q15_t)SGL_Q15_SHIFTDOWN(prod);  /* Q15 */
}

#endif  /* !SGL_FIXED_POINT__H__ */
