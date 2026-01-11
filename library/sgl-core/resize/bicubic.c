#include <stdint.h>
#include <stdlib.h>
#include "sgl-core.h"
#include "bicubic.h"

sgl_bicubic_lookup_t *sgl_generic_create_bicubic_lut(int32_t d_width, int32_t d_height, int32_t s_width, int32_t s_height)
{
    sgl_bicubic_lookup_t *lut;
    int32_t row, col;
    sgl_q11_ext_t x_step, y_step;
    sgl_q11_ext_t rx, ry;
    int32_t x1, y1;
    int32_t x2, y2;
    int32_t x3, y3;
    int32_t x4, y4;
    sgl_q11_t p, q;

    lut = (sgl_bicubic_lookup_t *)malloc(sizeof(sgl_bicubic_lookup_t));
    if (lut != NULL) {
        lut->col_lookup.x1      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        lut->col_lookup.x2      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        lut->col_lookup.x3      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        lut->col_lookup.x4      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        lut->col_lookup.p       = (sgl_q11_t *)malloc(sizeof(sgl_q11_t) * (size_t)d_width);

        lut->row_lookup.y1      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_height);
        lut->row_lookup.y2      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_height);
        lut->row_lookup.y3      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_height);
        lut->row_lookup.y4      = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_height);
        lut->row_lookup.q       = (sgl_q11_t *)malloc(sizeof(sgl_q11_t) * (size_t)d_height);

        if ((lut->col_lookup.x1 != NULL) && (lut->col_lookup.x2 != NULL) && (lut->col_lookup.x3 != NULL) && (lut->col_lookup.x4 != NULL) && (lut->col_lookup.p != NULL) &&
            (lut->row_lookup.y1 != NULL) && (lut->row_lookup.y2 != NULL) && (lut->row_lookup.y3 != NULL) && (lut->row_lookup.y4 != NULL) && (lut->row_lookup.q != NULL)) {
            /* create 'row' lookup table */
            y_step = SGL_INT_TO_Q11(s_height - 1) / (d_height - 1);
            for (row = 0; row < d_height; ++row) {
                ry = row * y_step;
                y2 = SGL_Q11_GET_INT_PART(ry);
                if (y2 >= (s_height - 1)) {
                    y2 = s_height - 1;
                }
                y1 = y2 - 1;
                if (y1 < 0) {
                    y1 = 0;
                }
                y3 = y2 + 1;
                if (y3 >= (s_height - 1)) {
                    y3 = s_height - 1;
                }
                y4 = y2 + 2;
                if (y4 >= (s_height - 1)) {
                    y4 = s_height - 1;
                }
                q = (sgl_q11_t)(ry - SGL_INT_TO_Q11(y2));

                lut->row_lookup.y1[row] = y1;
                lut->row_lookup.y2[row] = y2;
                lut->row_lookup.y3[row] = y3;
                lut->row_lookup.y4[row] = y4;
                lut->row_lookup.q[row] = q;
            }

            /* create 'column' lookup table */
            x_step = SGL_INT_TO_Q11(s_width - 1) / (d_width - 1);
            for (col = 0; col < d_width; ++col) {
                rx = col * x_step;
                x2 = SGL_Q11_GET_INT_PART(rx);
                if (x2 >= (s_width - 1)) {
                    x2 = s_width - 1;
                }
                x1 = x2 - 1;
                if (x1 < 0) {
                    x1 = 0;
                }
                x3 = x2 + 1;
                if (x3 >= (s_width - 1)) {
                    x3 = s_width - 1;
                }
                x4 = x2 + 2;
                if (x4 >= (s_width - 1)) {
                    x4 = s_width - 1;
                }
                p = (sgl_q11_t)(rx - SGL_INT_TO_Q11(x2));

                lut->col_lookup.x1[col] = x1;
                lut->col_lookup.x2[col] = x2;
                lut->col_lookup.x3[col] = x3;
                lut->col_lookup.x4[col] = x4;
                lut->col_lookup.p[col] = p;
            }

            lut->d_width = d_width;
            lut->d_height = d_height;
            lut->s_width = s_width;
            lut->s_height = s_height;
        }
        else {
            SGL_SAFE_FREE(lut->col_lookup.x1);
            SGL_SAFE_FREE(lut->col_lookup.x2);
            SGL_SAFE_FREE(lut->col_lookup.x3);
            SGL_SAFE_FREE(lut->col_lookup.x4);
            SGL_SAFE_FREE(lut->col_lookup.p);

            SGL_SAFE_FREE(lut->row_lookup.y1);
            SGL_SAFE_FREE(lut->row_lookup.y2);
            SGL_SAFE_FREE(lut->row_lookup.y3);
            SGL_SAFE_FREE(lut->row_lookup.y4);
            SGL_SAFE_FREE(lut->row_lookup.q);

            SGL_SAFE_FREE(lut);
        }
    }

    return lut;
}

void sgl_generic_destroy_bicubic_lut(sgl_bicubic_lookup_t *lut)
{
    if (lut != NULL) {
        SGL_SAFE_FREE(lut->col_lookup.x1);
        SGL_SAFE_FREE(lut->col_lookup.x2);
        SGL_SAFE_FREE(lut->col_lookup.x3);
        SGL_SAFE_FREE(lut->col_lookup.x4);
        SGL_SAFE_FREE(lut->col_lookup.p);

        SGL_SAFE_FREE(lut->row_lookup.y1);
        SGL_SAFE_FREE(lut->row_lookup.y2);
        SGL_SAFE_FREE(lut->row_lookup.y3);
        SGL_SAFE_FREE(lut->row_lookup.y4);
        SGL_SAFE_FREE(lut->row_lookup.q);

        free(lut);
    }
}
