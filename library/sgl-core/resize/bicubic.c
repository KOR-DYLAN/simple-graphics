/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
#include <sgl-core.h>
#include "bicubic.h"

static void sgl_generic_bicubic_lut_clear(sgl_bicubic_lookup_t *SGL_RESTRICT lut)
{
    lut->col_lookup.x1 = SGL_NULL;
    lut->col_lookup.x2 = SGL_NULL;
    lut->col_lookup.x3 = SGL_NULL;
    lut->col_lookup.x4 = SGL_NULL;
    lut->col_lookup.p = SGL_NULL;
    lut->row_lookup.y1 = SGL_NULL;
    lut->row_lookup.y2 = SGL_NULL;
    lut->row_lookup.y3 = SGL_NULL;
    lut->row_lookup.y4 = SGL_NULL;
    lut->row_lookup.q = SGL_NULL;
}

static void sgl_generic_bicubic_lut_release(sgl_bicubic_lookup_t *SGL_RESTRICT lut)
{
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
}

static sgl_bicubic_lookup_t *sgl_generic_bicubic_lut_allocate(void)
{
    sgl_bicubic_lookup_t *lut;

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut = (sgl_bicubic_lookup_t *)sgl_malloc(sizeof(sgl_bicubic_lookup_t));
    if (lut != SGL_NULL) {
        sgl_generic_bicubic_lut_clear(lut);
    }

    return lut;
}

static sgl_bool_t sgl_generic_bicubic_col_lookup_allocate(
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width)
{
    sgl_bool_t result = SGL_FALSE;

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->col_lookup.x1 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->col_lookup.x2 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->col_lookup.x3 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->col_lookup.x4 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->col_lookup.p = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_width);

    if ((lut->col_lookup.x1 != SGL_NULL) && (lut->col_lookup.x2 != SGL_NULL) &&
        (lut->col_lookup.x3 != SGL_NULL) && (lut->col_lookup.x4 != SGL_NULL) &&
        (lut->col_lookup.p != SGL_NULL))
    {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_generic_bicubic_row_lookup_allocate(
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_height)
{
    sgl_bool_t result = SGL_FALSE;

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->row_lookup.y1 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->row_lookup.y2 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->row_lookup.y3 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->row_lookup.y4 = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);
    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut->row_lookup.q = (sgl_q11_t *)sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_height);

    if ((lut->row_lookup.y1 != SGL_NULL) && (lut->row_lookup.y2 != SGL_NULL) &&
        (lut->row_lookup.y3 != SGL_NULL) && (lut->row_lookup.y4 != SGL_NULL) &&
        (lut->row_lookup.q != SGL_NULL))
    {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_generic_bicubic_lookup_allocate(
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width,
                sgl_int32_t d_height)
{
    sgl_bool_t result = SGL_FALSE;
    sgl_bool_t col_result;
    sgl_bool_t row_result;

    col_result = sgl_generic_bicubic_col_lookup_allocate(lut, d_width);
    row_result = sgl_generic_bicubic_row_lookup_allocate(lut, d_height);
    if ((col_result == SGL_TRUE) && (row_result == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

static void sgl_generic_bicubic_row_lookup_initialize(
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_height,
                sgl_int32_t s_height)
{
    sgl_int32_t row;
    sgl_q11_ext_t y_step;
    sgl_q11_ext_t ry;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_int32_t y3;
    sgl_int32_t y4;
    sgl_q11_t q;

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
}

static void sgl_generic_bicubic_col_lookup_initialize(
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width,
                sgl_int32_t s_width)
{
    sgl_int32_t col;
    sgl_q11_ext_t x_step;
    sgl_q11_ext_t rx;
    sgl_int32_t x1;
    sgl_int32_t x2;
    sgl_int32_t x3;
    sgl_int32_t x4;
    sgl_q11_t p;

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
}

static void sgl_generic_bicubic_lut_initialize(
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_int32_t s_width, sgl_int32_t s_height)
{
    /* Create row and column lookup tables. */
    sgl_generic_bicubic_row_lookup_initialize(lut, d_height, s_height);
    sgl_generic_bicubic_col_lookup_initialize(lut, d_width, s_width);

    lut->d_width = d_width;
    lut->d_height = d_height;
    lut->s_width = s_width;
    lut->s_height = s_height;
}

sgl_bicubic_lookup_t *sgl_generic_create_bicubic_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_bicubic_lookup_t *lut;

    lut = sgl_generic_bicubic_lut_allocate();
    if (lut != SGL_NULL) {
        if (sgl_generic_bicubic_lookup_allocate(lut, d_width, d_height) == SGL_TRUE) {
            sgl_generic_bicubic_lut_initialize(lut, d_width, d_height, s_width, s_height);
        }
        else {
            sgl_generic_bicubic_lut_release(lut);
            SGL_SAFE_FREE(lut);
        }
    }

    return lut;
}

void sgl_generic_destroy_bicubic_lut(sgl_bicubic_lookup_t *lut)
{
    if (lut != SGL_NULL) {
        sgl_generic_bicubic_lut_release(lut);
        sgl_free(lut);
    }
}
