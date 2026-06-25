/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
#include <sgl-core.h>
#include "bilinear.h"

sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_bilinear_lookup_t *lut;
    sgl_int32_t row;
    sgl_int32_t col;
    sgl_q11_ext_t x_step;
    sgl_q11_ext_t y_step;
    sgl_q11_ext_t rx;
    sgl_q11_ext_t ry;
    sgl_int32_t x1;
    sgl_int32_t y1;
    sgl_int32_t x2;
    sgl_int32_t y2;
    sgl_q11_t p;
    sgl_q11_t q;

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut = (sgl_bilinear_lookup_t *)sgl_malloc(sizeof(sgl_bilinear_lookup_t));
    if (lut != SGL_NULL) {
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.x1 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.x2 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.p = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->col_lookup.inv_p = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_width);

        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.y1 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.y2 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.q = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_height);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->row_lookup.inv_q = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_height);

        if ((lut->col_lookup.x1 != SGL_NULL) && (lut->col_lookup.x2 != SGL_NULL) && (lut->col_lookup.p != SGL_NULL) && (lut->col_lookup.inv_p != SGL_NULL) &&
            (lut->row_lookup.y1 != SGL_NULL) && (lut->row_lookup.y2 != SGL_NULL) && (lut->row_lookup.q != SGL_NULL) && (lut->row_lookup.inv_q != SGL_NULL)) {
            /* create 'row' lookup table */
            y_step = sgl_int_to_q11(s_height - 1) / (d_height - 1);
            for (row = 0; row < d_height; ++row) {
                ry = row * y_step;  /* Q11 */
                y1 = sgl_q11_get_int_part(ry);
                if (y1 >= (s_height - 1)) {
                    y1 = s_height - 1;
                }
                y2 = y1 + 1;
                if (y2 >= s_height) {
                    y2 = s_height - 1;
                }
                q = (sgl_q11_t)sgl_q11_get_frac_part(ry);
                if (y1 == (s_height - 1)) {
                    q = 0;
                }

                lut->row_lookup.y1[row] = y1;
                lut->row_lookup.y2[row] = y2;
                lut->row_lookup.q[row] = q;
                lut->row_lookup.inv_q[row] = (sgl_q11_t)SGL_Q11_ONE - q;
            }

            /* create 'column' lookup table */
            x_step = sgl_int_to_q11(s_width - 1) / (d_width - 1);
            for (col = 0; col < d_width; ++col) {
                rx = col * x_step;
                x1 = sgl_q11_get_int_part(rx);
                if (x1 >= (s_width - 1)) {
                    x1 = s_width - 1;
                }
                x2 = x1 + 1;
                if (x2 >= s_width) {
                    x2 = s_width - 1;
                }
                p = sgl_q11_get_frac_part(rx);
                if (x1 == (s_width - 1)) {
                    p = 0;
                }

                lut->col_lookup.x1[col] = x1;
                lut->col_lookup.x2[col] = x2;
                lut->col_lookup.p[col] = p;
                lut->col_lookup.inv_p[col] = (sgl_q11_t)SGL_Q11_ONE - p;
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

            SGL_SAFE_FREE(lut);
        }
    }

    return lut;
}

void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut)
{
    if (lut != SGL_NULL) {
        SGL_SAFE_FREE(lut->col_lookup.x1);
        SGL_SAFE_FREE(lut->col_lookup.x2);
        SGL_SAFE_FREE(lut->col_lookup.p);
        SGL_SAFE_FREE(lut->col_lookup.inv_p);

        SGL_SAFE_FREE(lut->row_lookup.y1);
        SGL_SAFE_FREE(lut->row_lookup.y2);
        SGL_SAFE_FREE(lut->row_lookup.q);
        SGL_SAFE_FREE(lut->row_lookup.inv_q);

        sgl_free(lut);
    }
}
