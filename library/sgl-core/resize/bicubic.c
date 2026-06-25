/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
#include <stdint.h>
#include <stdlib.h>
#include <sgl-core.h>
#include "bicubic.h"

sgl_bicubic_lookup_t *sgl_generic_create_bicubic_lut(int32_t d_width, int32_t d_height, int32_t s_width, int32_t s_height)
{
    sgl_bicubic_lookup_t *lut;
    int32_t row;
    int32_t col;
    sgl_q11_ext_t x_step;
    sgl_q11_ext_t y_step;
    sgl_q11_ext_t rx;
    sgl_q11_ext_t ry;
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t x3;
    int32_t y3;
    int32_t x4;
    int32_t y4;
    sgl_q11_t p;
    sgl_q11_t q;

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut = (sgl_bicubic_lookup_t *)sgl_malloc(sizeof(sgl_bicubic_lookup_t));
    if (lut != NULL) {
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.x1 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.x2 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.x3 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.x4 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.p = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (size_t)d_width);

        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.y1 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.y2 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.y3 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.y4 = (int32_t *)sgl_malloc(sizeof(int32_t) * (size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.q = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (size_t)d_height);

        if ((lut->col_lookup.x1 != NULL) && (lut->col_lookup.x2 != NULL) && (lut->col_lookup.x3 != NULL) && (lut->col_lookup.x4 != NULL) && (lut->col_lookup.p != NULL) &&
            (lut->row_lookup.y1 != NULL) && (lut->row_lookup.y2 != NULL) && (lut->row_lookup.y3 != NULL) && (lut->row_lookup.y4 != NULL) && (lut->row_lookup.q != NULL)) {
            /* create 'row' lookup table */
            y_step = sgl_int_to_q11(s_height - 1) / (d_height - 1);
            for (row = 0; row < d_height; ++row) {
                ry = row * y_step;
                y2 = sgl_q11_get_int_part(ry);
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
                q = (sgl_q11_t)(ry - sgl_int_to_q11(y2));

                lut->row_lookup.y1[row] = y1;
                lut->row_lookup.y2[row] = y2;
                lut->row_lookup.y3[row] = y3;
                lut->row_lookup.y4[row] = y4;
                lut->row_lookup.q[row] = q;
            }

            /* create 'column' lookup table */
            x_step = sgl_int_to_q11(s_width - 1) / (d_width - 1);
            for (col = 0; col < d_width; ++col) {
                rx = col * x_step;
                x2 = sgl_q11_get_int_part(rx);
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
                p = (sgl_q11_t)(rx - sgl_int_to_q11(x2));

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

        sgl_free(lut);
    }
}
