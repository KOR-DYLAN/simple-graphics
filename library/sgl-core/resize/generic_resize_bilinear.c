#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"

#define FP_BITS 15
#define FP_ONE  (1 << FP_BITS)
#define FP_MASK (FP_ONE - 1)

typedef struct {
    int32_t x1_off, x2_off;
    int32_t p, inv_p;   /* Q15 */
} bilinear_column_lookup_t;

typedef struct {
    int32_t y1, y2;
    int32_t q, inv_q;   /* Q15 */
} bilinear_row_lookup_t;

static inline int32_t mul_q15(int32_t a, int32_t b)
{
    /* Avoid UB(Undefined Behavior) by changing 'signed' to 'unsigned'. */
    uint32_t ua = (uint32_t)a;
    uint32_t ub = (uint32_t)b;
    uint32_t prod = ua * ub;                /* Q30 */

    prod += (uint32_t)(1 << (FP_BITS - 1)); /* Rounding */

    return (int32_t)(prod >> FP_BITS);      /* Q15 */
}

static inline uint8_t clamp_u8_i32(int32_t val)
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

sgl_result_t sgl_generic_resize_bilinear(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row, col;
    int32_t x_step, y_step;
    int32_t rx, ry;
    int32_t x1, y1;
    int32_t x1_off, x2_off;
    int32_t x2, y2;
    int32_t p, inv_p;
    int32_t q, inv_q;
    int32_t w00, w01, w10, w11;
    int32_t acc, value;
    bilinear_column_lookup_t *column_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t ch, src_stride;
    uint8_t *dst_buf = dst;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *src_y1x1, *src_y1x2;
    uint8_t *src_y2x1, *src_y2x2;
    int32_t errcnt = 0;

    /* check buffer address */
    if ((dst == NULL) || (src == NULL)) {
        errcnt += 1;
    }

    /* check boundary */
    if ((d_width <= 0) || (d_height <= 0) || (s_width <= 0) || (s_height <= 0)) {
        errcnt += 1;
    }

    /* check bpp(bytes per pixel) */
    if (bpp <= 0) {
        errcnt += 1;
    }

    if (errcnt == 0) {
        row_lookup = (bilinear_row_lookup_t *)malloc(sizeof(bilinear_row_lookup_t) * (size_t)d_width);
        column_lookup = (bilinear_column_lookup_t *)malloc(sizeof(bilinear_column_lookup_t) * (size_t)d_width);
        if ((column_lookup != NULL) && (row_lookup != NULL)) {
            /* create 'row' lookup table */
            y_step = ((s_height - 1) << FP_BITS) / (d_height - 1);
            for (row = 0; row < d_height; ++row) {
                ry = row * y_step;  /* Q15 */
                y1 = ry >> FP_BITS;
                if (y1 >= (s_height - 1)) {
                    y1 = s_height - 1;
                }
                y2 = y1 + 1;
                if (y2 >= s_height) {
                    y2 = s_height - 1;
                }
                q = ry & FP_MASK;
                if (y1 == (s_height - 1)) {
                    q = 0;
                }

                row_lookup[row].y1 = y1;
                row_lookup[row].y2 = y2;
                row_lookup[row].q = q;
                row_lookup[row].inv_q = FP_ONE - q;
            }

            /* create 'column' lookup table */
            x_step = ((s_width - 1) << FP_BITS) / (d_width - 1);
            for (col = 0; col < d_width; ++col) {
                rx = col * x_step;
                x1 = rx >> FP_BITS;
                if (x1 >= (s_width - 1)) {
                    x1 = s_width - 1;
                }
                x2 = x1 + 1;
                if (x2 >= s_width) {
                    x2 = s_width - 1;
                }
                p = rx & FP_MASK;
                if (x1 == (s_width - 1)) {
                    p = 0;
                }

                column_lookup[col].x1_off = x1 * bpp;
                column_lookup[col].x2_off = x2 * bpp;
                column_lookup[col].p = p;
                column_lookup[col].inv_p = FP_ONE - p;
            }

            /* resize */
            src_stride = s_width * bpp;
            for (row = 0; row < d_height; ++row) {
                y1 = row_lookup[row].y1;
                y2 = row_lookup[row].y2;
                q = row_lookup[row].q;
                inv_q = row_lookup[row].inv_q;

                src_y1_buf = src + (y1 * src_stride);
                src_y2_buf = src + (y2 * src_stride);

                for (col = 0; col < d_width; ++col) {
                    x1_off = column_lookup[col].x1_off;
                    x2_off = column_lookup[col].x2_off;
                    p = column_lookup[col].p;
                    inv_p = column_lookup[col].inv_p;

                    w00 = mul_q15(inv_p, inv_q); /* Q15 */
                    w01 = mul_q15(    p, inv_q); /* Q15 */
                    w10 = mul_q15(inv_p,     q); /* Q15 */
                    w11 = mul_q15(    p,     q); /* Q15 */

                    src_y1x1 = src_y1_buf + x1_off;
                    src_y1x2 = src_y1_buf + x2_off;
                    src_y2x1 = src_y2_buf + x1_off;
                    src_y2x2 = src_y2_buf + x2_off;
                    for (ch = 0; ch < bpp; ++ch) {
                        acc = (w00 * src_y1x1[ch]) + 
                              (w01 * src_y1x2[ch]) +
                              (w10 * src_y2x1[ch]) + 
                              (w11 * src_y2x2[ch]);
                        value = (acc + (1 << (FP_BITS - 1))) >> FP_BITS; /* Rounding */

                        /* Q15 -> u8 */
                        dst_buf[ch] = clamp_u8_i32(value);
                    }
                    dst_buf += bpp;
                }
            }

            free(row_lookup);
            free(column_lookup);
        }
        else {
            SGL_SAFE_FREE(row_lookup);
            SGL_SAFE_FREE(column_lookup);
            result = SGL_ERROR_MEMORY_ALLOCATION;
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}
