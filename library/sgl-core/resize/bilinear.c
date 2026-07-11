/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "bilinear.h"

static void sgl_generic_bilinear_lut_clear(sgl_bilinear_lookup_t *SGL_RESTRICT lut)
{
    lut->col_lookup.x1 = SGL_NULL;
    lut->col_lookup.x2 = SGL_NULL;
    lut->col_lookup.p = SGL_NULL;
    lut->col_lookup.inv_p = SGL_NULL;
    lut->row_lookup.y1 = SGL_NULL;
    lut->row_lookup.y2 = SGL_NULL;
    lut->row_lookup.q = SGL_NULL;
    lut->row_lookup.inv_q = SGL_NULL;
}

static void sgl_generic_bilinear_lut_release(sgl_bilinear_lookup_t *SGL_RESTRICT lut)
{
    SGL_SAFE_FREE(lut->col_lookup.x1);
    SGL_SAFE_FREE(lut->col_lookup.x2);
    SGL_SAFE_FREE(lut->col_lookup.p);
    SGL_SAFE_FREE(lut->col_lookup.inv_p);

    SGL_SAFE_FREE(lut->row_lookup.y1);
    SGL_SAFE_FREE(lut->row_lookup.y2);
    SGL_SAFE_FREE(lut->row_lookup.q);
    SGL_SAFE_FREE(lut->row_lookup.inv_q);
}

static sgl_bilinear_lookup_t *sgl_generic_bilinear_lut_allocate(void)
{
    sgl_bilinear_lookup_t *lut;

    lut = sgl_memory_as_bilinear_lookup(sgl_malloc(sizeof(sgl_bilinear_lookup_t)));
    if (lut != SGL_NULL) {
        sgl_generic_bilinear_lut_clear(lut);
    }

    return lut;
}

static sgl_bool_t sgl_generic_bilinear_col_lookup_allocate(
                sgl_bilinear_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width)
{
    sgl_bool_t result = SGL_FALSE;

    lut->col_lookup.x1 = sgl_memory_as_int32(sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width));
    lut->col_lookup.x2 = sgl_memory_as_int32(sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width));
    lut->col_lookup.p = sgl_memory_as_q11(sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_width));
    lut->col_lookup.inv_p = sgl_memory_as_q11(sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_width));

    if ((lut->col_lookup.x1 != SGL_NULL) && (lut->col_lookup.x2 != SGL_NULL) &&
        (lut->col_lookup.p != SGL_NULL) && (lut->col_lookup.inv_p != SGL_NULL))
    {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_generic_bilinear_row_lookup_allocate(
                sgl_bilinear_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_height)
{
    sgl_bool_t result = SGL_FALSE;

    lut->row_lookup.y1 = sgl_memory_as_int32(sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height));
    lut->row_lookup.y2 = sgl_memory_as_int32(sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height));
    lut->row_lookup.q = sgl_memory_as_q11(sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_height));
    lut->row_lookup.inv_q = sgl_memory_as_q11(sgl_malloc(sizeof(sgl_q11_t) * (sgl_size_t)d_height));

    if ((lut->row_lookup.y1 != SGL_NULL) && (lut->row_lookup.y2 != SGL_NULL) &&
        (lut->row_lookup.q != SGL_NULL) && (lut->row_lookup.inv_q != SGL_NULL))
    {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_generic_bilinear_lookup_allocate(
                sgl_bilinear_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width,
                sgl_int32_t d_height)
{
    sgl_bool_t result = SGL_FALSE;
    sgl_bool_t col_result;
    sgl_bool_t row_result;

    col_result = sgl_generic_bilinear_col_lookup_allocate(lut, d_width);
    row_result = sgl_generic_bilinear_row_lookup_allocate(lut, d_height);
    if ((col_result == SGL_TRUE) && (row_result == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

static void sgl_generic_bilinear_row_lookup_initialize(
                sgl_bilinear_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_height,
                sgl_int32_t s_height)
{
    sgl_int32_t row;
    sgl_q11_ext_t y_step;
    sgl_q11_ext_t ry;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_q11_t q;

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
}

static void sgl_generic_bilinear_col_lookup_initialize(
                sgl_bilinear_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width,
                sgl_int32_t s_width)
{
    sgl_int32_t col;
    sgl_q11_ext_t x_step;
    sgl_q11_ext_t rx;
    sgl_int32_t x1;
    sgl_int32_t x2;
    sgl_q11_t p;

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
}

static void sgl_generic_bilinear_lut_initialize(
                sgl_bilinear_lookup_t *SGL_RESTRICT lut,
                sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_int32_t s_width, sgl_int32_t s_height)
{
    /* Create row and column lookup tables. */
    sgl_generic_bilinear_row_lookup_initialize(lut, d_height, s_height);
    sgl_generic_bilinear_col_lookup_initialize(lut, d_width, s_width);

    lut->d_width = d_width;
    lut->d_height = d_height;
    lut->s_width = s_width;
    lut->s_height = s_height;
}

sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_bilinear_lookup_t *lut;

    lut = sgl_generic_bilinear_lut_allocate();
    if (lut != SGL_NULL) {
        if (sgl_generic_bilinear_lookup_allocate(lut, d_width, d_height) == SGL_TRUE) {
            sgl_generic_bilinear_lut_initialize(lut, d_width, d_height, s_width, s_height);
        }
        else {
            sgl_generic_bilinear_lut_release(lut);
            SGL_SAFE_FREE(lut);
        }
    }

    return lut;
}

void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut)
{
    if (lut != SGL_NULL) {
        sgl_generic_bilinear_lut_release(lut);
        sgl_free(lut);
    }
}
