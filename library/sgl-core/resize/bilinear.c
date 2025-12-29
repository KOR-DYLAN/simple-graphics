#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"
#include "bilinear.h"

sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(int32_t d_width, int32_t d_height, int32_t s_width, int32_t s_height)
{
    sgl_bilinear_lookup_t *lut;
    int32_t row, col;
    sgl_q15_t x_step, y_step;
    sgl_q15_t rx, ry;
    int32_t x1, y1;
    int32_t x2, y2;
    sgl_q15_t p, q;

    lut = (sgl_bilinear_lookup_t *)malloc(sizeof(sgl_bilinear_lookup_t));
    if (lut != NULL) {
        lut->col_lookup.x1      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        lut->col_lookup.x2      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        lut->col_lookup.p       = (sgl_q15_t *)malloc(sizeof(sgl_q15_t) * (size_t)d_width);
        lut->col_lookup.inv_p   = (sgl_q15_t *)malloc(sizeof(sgl_q15_t) * (size_t)d_width);

        lut->row_lookup.y1      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_height);
        lut->row_lookup.y2      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_height);
        lut->row_lookup.q       = (sgl_q15_t *)malloc(sizeof(sgl_q15_t) * (size_t)d_height);
        lut->row_lookup.inv_q   = (sgl_q15_t *)malloc(sizeof(sgl_q15_t) * (size_t)d_height);

        if ((lut->col_lookup.x1 != NULL) && (lut->col_lookup.x2 != NULL) && (lut->col_lookup.p != NULL) && (lut->col_lookup.inv_p != NULL) &&
            (lut->row_lookup.y1 != NULL) && (lut->row_lookup.y2 != NULL) && (lut->row_lookup.q != NULL) && (lut->row_lookup.inv_q != NULL)) {
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

                lut->row_lookup.y1[row] = y1;
                lut->row_lookup.y2[row] = y2;
                lut->row_lookup.q[row] = q;
                lut->row_lookup.inv_q[row] = SGL_Q15_ONE - q;
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

                lut->col_lookup.x1[col] = x1;
                lut->col_lookup.x2[col] = x2;
                lut->col_lookup.p[col] = p;
                lut->col_lookup.inv_p[col] = SGL_Q15_ONE - p;
            }

            lut->d_width = d_width;
            lut->d_height = d_height;
            lut->s_width = s_width;
            lut->s_height = s_height;
        }
        else {
            SGL_SAFE_FREE(lut->col_lookup.x1);
            SGL_SAFE_FREE(lut->col_lookup.x2);
            SGL_SAFE_FREE(lut->col_lookup.p);
            SGL_SAFE_FREE(lut->col_lookup.inv_p);

            SGL_SAFE_FREE(lut->row_lookup.y1);
            SGL_SAFE_FREE(lut->row_lookup.y2);
            SGL_SAFE_FREE(lut->row_lookup.q);
            SGL_SAFE_FREE(lut->row_lookup.inv_q);
        }
    }

    return lut;
}

void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut)
{
    if (lut != NULL) {
        SGL_SAFE_FREE(lut->col_lookup.x1);
        SGL_SAFE_FREE(lut->col_lookup.x2);
        SGL_SAFE_FREE(lut->col_lookup.p);
        SGL_SAFE_FREE(lut->col_lookup.inv_p);

        SGL_SAFE_FREE(lut->row_lookup.y1);
        SGL_SAFE_FREE(lut->row_lookup.y2);
        SGL_SAFE_FREE(lut->row_lookup.q);
        SGL_SAFE_FREE(lut->row_lookup.inv_q);

        free(lut);
    }
}
