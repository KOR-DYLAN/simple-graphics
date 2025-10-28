#include <stdint.h>
#include <stdlib.h>
#include "sgl-fixed_point.h"
#include "sgl.h"

typedef struct {
    int32_t x1_off, x2_off;
    sgl_q15_t p, inv_p;   /* Q15 */
} bilinear_column_lookup_t;

typedef struct {
    int32_t y1, y2;
    sgl_q15_t q, inv_q;   /* Q15 */
} bilinear_row_lookup_t;

sgl_result_t sgl_generic_resize_bilinear(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row, col;
    sgl_q15_t x_step, y_step;
    sgl_q15_t rx, ry;
    int32_t x1, y1;
    int32_t x1_off, x2_off;
    int32_t x2, y2;
    sgl_q15_t p, inv_p;
    sgl_q15_t q, inv_q;
    sgl_q15_t w00, w01, w10, w11;
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
            y_step = SGL_INT_TO_Q15(s_height - 1) / (d_height - 1);
            for (row = 0; row < d_height; ++row) {
                ry = row * y_step;  /* Q15 */
                y1 = SGL_Q15_GET_INT_PART(ry);
                if (y1 >= (s_height - 1)) {
                    y1 = s_height - 1;
                }
                y2 = y1 + 1;
                if (y2 >= s_height) {
                    y2 = s_height - 1;
                }
                q = SGL_Q15_GET_FRAC_PART(ry);
                if (y1 == (s_height - 1)) {
                    q = 0;
                }

                row_lookup[row].y1 = y1;
                row_lookup[row].y2 = y2;
                row_lookup[row].q = q;
                row_lookup[row].inv_q = SGL_Q15_ONE - q;
            }

            /* create 'column' lookup table */
            x_step = SGL_INT_TO_Q15(s_width - 1) / (d_width - 1);
            for (col = 0; col < d_width; ++col) {
                rx = col * x_step;
                x1 = SGL_Q15_GET_INT_PART(rx);
                if (x1 >= (s_width - 1)) {
                    x1 = s_width - 1;
                }
                x2 = x1 + 1;
                if (x2 >= s_width) {
                    x2 = s_width - 1;
                }
                p = SGL_Q15_GET_FRAC_PART(rx);
                if (x1 == (s_width - 1)) {
                    p = 0;
                }

                column_lookup[col].x1_off = x1 * bpp;
                column_lookup[col].x2_off = x2 * bpp;
                column_lookup[col].p = p;
                column_lookup[col].inv_p = SGL_Q15_ONE - p;
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

                    w00 = sgl_q15_mul(inv_p, inv_q); /* Q15 */
                    w01 = sgl_q15_mul(    p, inv_q); /* Q15 */
                    w10 = sgl_q15_mul(inv_p,     q); /* Q15 */
                    w11 = sgl_q15_mul(    p,     q); /* Q15 */

                    src_y1x1 = src_y1_buf + x1_off;
                    src_y1x2 = src_y1_buf + x2_off;
                    src_y2x1 = src_y2_buf + x1_off;
                    src_y2x2 = src_y2_buf + x2_off;
                    for (ch = 0; ch < bpp; ++ch) {
                        acc = (w00 * src_y1x1[ch]) + 
                              (w01 * src_y1x2[ch]) +
                              (w10 * src_y2x1[ch]) + 
                              (w11 * src_y2x2[ch]);
                        value = SGL_Q15_SHIFTDOWN(SGL_Q15_ROUNDUP(acc));

                        /* Q15 -> u8 */
                        dst_buf[ch] = sgl_clamp_u8_i32(value);
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
