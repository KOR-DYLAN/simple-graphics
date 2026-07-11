/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "bicubic.h"

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_generic_bicubic_interpolation(sgl_q11_ext_t v1, sgl_q11_ext_t v2, sgl_q11_ext_t v3, sgl_q11_ext_t v4, sgl_q11_ext_t d)
{
    sgl_q11_ext_t v;
    sgl_q11_ext_t p1;
    sgl_q11_ext_t p2;
    sgl_q11_ext_t p3;
    sgl_q11_ext_t p4;

    p1 = 2 * v2;
    p2 = -v1 + v3;
    p3 = (2 * v1) - (5 * v2) + (4 * v3) - v4;
    p4 = -v1 + (3 * v2) - (3 * v3) + v4;

    v = p3 + sgl_q11_ext_mul(d, p4);
    v = p2 + sgl_q11_ext_mul(d, v);
    v = p1 + sgl_q11_ext_mul(d, v);

    return v / 2;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bicubic_line_stripe(sgl_int32_t row, sgl_bicubic_data_t *data) {
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t d_width;
    sgl_int32_t bpp;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_int32_t x3_off;
    sgl_int32_t x4_off;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_int32_t y3;
    sgl_int32_t y4;
    sgl_q11_t p;
    sgl_q11_t q;
    sgl_q11_ext_t v1;
    sgl_q11_ext_t v2;
    sgl_q11_ext_t v3;
    sgl_q11_ext_t v4;
    sgl_q11_ext_t value;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t ch;
    sgl_int32_t src_stride;
    sgl_int32_t dst_stride;
    const sgl_uint8_t *src_y1_buf;
    const sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y3_buf;
    const sgl_uint8_t *src_y4_buf;
    const sgl_uint8_t *src_y1x1;
    const sgl_uint8_t *src_y1x2;
    const sgl_uint8_t *src_y1x3;
    const sgl_uint8_t *src_y1x4;
    const sgl_uint8_t *src_y2x1;
    const sgl_uint8_t *src_y2x2;
    const sgl_uint8_t *src_y2x3;
    const sgl_uint8_t *src_y2x4;
    const sgl_uint8_t *src_y3x1;
    const sgl_uint8_t *src_y3x2;
    const sgl_uint8_t *src_y3x3;
    const sgl_uint8_t *src_y3x4;
    const sgl_uint8_t *src_y4x1;
    const sgl_uint8_t *src_y4x2;
    const sgl_uint8_t *src_y4x3;
    const sgl_uint8_t *src_y4x4;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    d_width = data->lut->d_width;
    bpp = data->bpp;

    /* set 'row' data */
    y1 = row_lookup->y1[row];
    y2 = row_lookup->y2[row];
    y3 = row_lookup->y3[row];
    y4 = row_lookup->y4[row];
    q = row_lookup->q[row];

    src_stride = data->src_stride;
    src = data->src;
    src_y1_buf = &src[y1 * src_stride];
    src_y2_buf = &src[y2 * src_stride];
    src_y3_buf = &src[y3 * src_stride];
    src_y4_buf = &src[y4 * src_stride];

    dst_stride = data->dst_stride;
    dst = &data->dst[row * dst_stride];

    for (col = 0; col < d_width; ++col) {
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        x3_off = col_lookup->x3[col] * bpp;
        x4_off = col_lookup->x4[col] * bpp;
        p = col_lookup->p[col];

        src_y1x1 = &src_y1_buf[x1_off];
        src_y1x2 = &src_y1_buf[x2_off];
        src_y1x3 = &src_y1_buf[x3_off];
        src_y1x4 = &src_y1_buf[x4_off];

        src_y2x1 = &src_y2_buf[x1_off];
        src_y2x2 = &src_y2_buf[x2_off];
        src_y2x3 = &src_y2_buf[x3_off];
        src_y2x4 = &src_y2_buf[x4_off];

        src_y3x1 = &src_y3_buf[x1_off];
        src_y3x2 = &src_y3_buf[x2_off];
        src_y3x3 = &src_y3_buf[x3_off];
        src_y3x4 = &src_y3_buf[x4_off];

        src_y4x1 = &src_y4_buf[x1_off];
        src_y4x2 = &src_y4_buf[x2_off];
        src_y4x3 = &src_y4_buf[x3_off];
        src_y4x4 = &src_y4_buf[x4_off];

        for (ch = 0; ch < bpp; ++ch) {
            v1 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y1x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y1x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y1x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y1x4[ch]), (sgl_q11_ext_t)p);
            v2 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y2x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y2x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y2x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y2x4[ch]), (sgl_q11_ext_t)p);
            v3 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y3x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y3x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y3x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y3x4[ch]), (sgl_q11_ext_t)p);
            v4 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y4x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y4x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y4x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y4x4[ch]), (sgl_q11_ext_t)p);
            value = sgl_generic_bicubic_interpolation(v1, v2, v3, v4, (sgl_q11_ext_t)q);
            value = sgl_q11_shift_down(sgl_q11_round_up(value));

            /* Q11 -> u8 */
            dst[ch] = sgl_clamp_u8_i32(value);
        }
        dst = &dst[bpp];
    }
}

static sgl_int32_t sgl_generic_resize_bicubic_count_errors(
    const sgl_uint8_t *dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    const sgl_uint8_t *src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp)
{
    sgl_int32_t errcnt;

    errcnt = 0;

    /* check buffer address */
    if ((dst == SGL_NULL) || (src == SGL_NULL)) {
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

    return errcnt;
}

static sgl_bicubic_lookup_t *sgl_generic_resize_bicubic_select_lut(
    sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
    sgl_bicubic_lookup_t **temp_lut,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_bicubic_lookup_t *lut;

    lut = SGL_NULL;
    if (ext_lut != SGL_NULL) {
        if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
            (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
        {
            /* apply external look-up table */
            lut = ext_lut;
        }
    }

    if (lut == SGL_NULL) {
        /* create temp look-up table */
        *temp_lut = sgl_generic_create_bicubic_lut(
            d_width, d_height, s_width, s_height);
        lut = *temp_lut;
    }

    return lut;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bicubic_set_data(
    sgl_bicubic_data_t *data,
    sgl_bicubic_lookup_t *lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t d_width,
    sgl_int32_t bpp)
{
    data->bpp = bpp;
    data->src = src;
    data->dst = dst;
    data->lut = lut;
    data->src_stride = s_width * bpp;
    data->dst_stride = d_width * bpp;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bicubic_single(
    sgl_bicubic_data_t *data,
    sgl_int32_t d_height)
{
    sgl_int32_t row;

    for (row = 0; row < d_height; ++row) {
        sgl_generic_resize_bicubic_line_stripe(row, data);
    }
}

#if defined(SGL_CFG_HAS_THREAD)
static sgl_result_t sgl_generic_resize_bicubic_threaded(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bicubic_data_t *data,
    sgl_int32_t d_height)
{
    sgl_result_t result;
    sgl_bicubic_current_t *currents;
    sgl_queue_t *operations;
    sgl_int32_t i;
    sgl_int32_t num_operations;
    sgl_int32_t mod_operations;

    result = SGL_SUCCESS;
    currents = SGL_NULL;
    operations = SGL_NULL;
    num_operations = d_height / SGL_GENERIC_BULK_SIZE;
    mod_operations = d_height % SGL_GENERIC_BULK_SIZE;
    if (mod_operations != 0) {
        num_operations += 1;
    }

    operations = sgl_queue_create((sgl_size_t)num_operations);
    currents = sgl_memory_as_bicubic_current(sgl_malloc(
        sizeof(sgl_bicubic_current_t) * (sgl_size_t)num_operations));
    if ((operations != SGL_NULL) && (currents != SGL_NULL)) {
        for (i = 0; i < num_operations; ++i) {
            currents[i].row = i * SGL_GENERIC_BULK_SIZE;
            currents[i].count = SGL_GENERIC_BULK_SIZE;
            (void)sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
        }

        if (mod_operations != 0) {
            currents[num_operations - 1].count = mod_operations;
        }

        /* multi-threaded resize */
        (void)sgl_threadpool_attach_routine(
            pool,
            sgl_generic_resize_bicubic_routine,
            operations,
            (void *)data);
        sgl_queue_destroy(&operations);
    }
    else {
        result = SGL_ERROR_MEMORY_ALLOCATION;
    }

    SGL_SAFE_FREE(currents);
    SGL_SAFE_FREE(operations);

    return result;
}
#endif  /* !SGL_CFG_HAS_THREAD */

static sgl_result_t sgl_generic_resize_bicubic_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bicubic_data_t *data,
    sgl_int32_t d_height)
{
    sgl_result_t result;

    result = SGL_SUCCESS;
    if (pool == SGL_NULL) {
        sgl_generic_resize_bicubic_single(data, d_height);
    }
#if defined(SGL_CFG_HAS_THREAD)
    else {
        result = sgl_generic_resize_bicubic_threaded(pool, data, d_height);
    }
#else
    else {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
#endif  /* !SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_generic_resize_bicubic(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_bicubic_data_t data;
    sgl_bicubic_lookup_t *lut = SGL_NULL;
    sgl_bicubic_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;

    errcnt = sgl_generic_resize_bicubic_count_errors(
        dst, d_width, d_height, src, s_width, s_height, bpp);

    /* check error count */
    if (errcnt != 0) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }
    else {
        lut = sgl_generic_resize_bicubic_select_lut(
            ext_lut, &temp_lut, d_width, d_height, s_width, s_height);
        if (lut != SGL_NULL) {
            sgl_generic_resize_bicubic_set_data(
                &data, lut, dst, src, s_width, d_width, bpp);
            result = sgl_generic_resize_bicubic_run(pool, &data, d_height);

            if (temp_lut != SGL_NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_bicubic_lut(temp_lut);
            }
        }
    }

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_bicubic_current_t *cur = sgl_memory_as_const_bicubic_current(current);
    sgl_bicubic_data_t *data = sgl_memory_as_bicubic_data(cookie);
    sgl_int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_generic_resize_bicubic_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
