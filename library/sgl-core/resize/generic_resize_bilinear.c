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
#include "resize_bitops.h"
#include "sgl_trace.h"
#include "threaded_resize.h"

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

#define SGL_BILINEAR_PAIR_SHIFT (32U)

enum {
    SGL_GENERIC_BILINEAR_CACHE_BULK_SIZE = 32,
    SGL_GENERIC_BILINEAR_ROW_CACHE_COUNT = 2,
    SGL_GENERIC_BILINEAR_ROW_CACHE_MASK =
        SGL_GENERIC_BILINEAR_ROW_CACHE_COUNT - 1,
    SGL_GENERIC_BILINEAR_SEPARABLE_BITS = SGL_Q11_FRAC_BITS * 2,
    SGL_GENERIC_BILINEAR_SEPARABLE_HALF =
        1 << (SGL_GENERIC_BILINEAR_SEPARABLE_BITS - 1)
};

typedef struct {
    sgl_int32_t y;
    sgl_q11_ext_t *SGL_RESTRICT row;
} sgl_generic_bilinear_row_cache_t;

/* Pixel and cached interpolation values are nonnegative and range-bounded. */
static SGL_ALWAYS_INLINE sgl_q11_ext_t
sgl_generic_bilinear_scale_nonnegative_q11(sgl_q11_ext_t value)
{
    return value << SGL_Q11_FRAC_BITS;
}

/*
 * Design and Operation
 * --------------------
 * The direct bpp32 fallback keeps two byte channels packed into independent
 * 32-bit lanes of one 64-bit accumulator.
 *
 *   bit 63                    32 31                     0
 *      +-----------------------+-----------------------+
 *      | channel N+1 acc (Q11) | channel N acc (Q11)   |
 *      +-----------------------+-----------------------+
 *
 * A lane is bounded by sum(weight * 255), so it cannot carry into the neighbor
 * lane.  The final store applies the same Q11 rounding and u8 clamp as the
 * scalar channel loop.
 */
static SGL_ALWAYS_INLINE sgl_uint8_t sgl_generic_bilinear_acc_to_u8(
    sgl_uint32_t acc)
{
    sgl_q11_ext_t value;
    sgl_uint8_t result;

    value = sgl_q11_shift_down(sgl_q11_round_up((sgl_q11_ext_t)acc));
    result = sgl_clamp_u8_i32(value);

    return result;
}

static SGL_ALWAYS_INLINE sgl_uint64_t sgl_generic_bilinear_load_pair(
    const sgl_uint8_t *src)
{
    sgl_uint64_t pair;

    pair = (sgl_uint64_t)src[0];
    pair |= ((sgl_uint64_t)src[1] << SGL_BILINEAR_PAIR_SHIFT);

    return pair;
}

static SGL_ALWAYS_INLINE sgl_uint64_t sgl_generic_bilinear_interpolate_pair(
    sgl_q11_t w00, sgl_q11_t w01, sgl_q11_t w10, sgl_q11_t w11,
    const sgl_uint8_t *src_y1x1, const sgl_uint8_t *src_y1x2,
    const sgl_uint8_t *src_y2x1, const sgl_uint8_t *src_y2x2)
{
    sgl_uint64_t acc;

    acc = ((sgl_uint64_t)(sgl_uint32_t)w00 *
           sgl_generic_bilinear_load_pair(src_y1x1)) +
          ((sgl_uint64_t)(sgl_uint32_t)w01 *
           sgl_generic_bilinear_load_pair(src_y1x2)) +
          ((sgl_uint64_t)(sgl_uint32_t)w10 *
           sgl_generic_bilinear_load_pair(src_y2x1)) +
          ((sgl_uint64_t)(sgl_uint32_t)w11 *
           sgl_generic_bilinear_load_pair(src_y2x2));

    return acc;
}

static SGL_ALWAYS_INLINE void sgl_generic_bilinear_store_pair(
    sgl_uint8_t *dst, sgl_uint64_t acc)
{
    sgl_uint32_t low_acc;
    sgl_uint32_t high_acc;

    low_acc = (sgl_uint32_t)(acc & (sgl_uint64_t)0xFFFFFFFFU);
    high_acc = (sgl_uint32_t)(acc >> SGL_BILINEAR_PAIR_SHIFT);
    dst[0] = sgl_generic_bilinear_acc_to_u8(low_acc);
    dst[1] = sgl_generic_bilinear_acc_to_u8(high_acc);
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_line_stripe_bpp32(
    sgl_int32_t row, sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t d_width;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_q11_t p;
    sgl_q11_t inv_p;
    sgl_q11_t q;
    sgl_q11_t inv_q;
    sgl_q11_t w00;
    sgl_q11_t w01;
    sgl_q11_t w10;
    sgl_q11_t w11;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t src_stride;
    sgl_int32_t dst_stride;
    const sgl_uint8_t *src_y1_buf;
    const sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y1x1;
    const sgl_uint8_t *src_y1x2;
    const sgl_uint8_t *src_y2x1;
    const sgl_uint8_t *src_y2x2;
    sgl_uint64_t pair;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    d_width = data->lut->d_width;

    /* set 'row' data */
    y1 = row_lookup->y1[row];
    y2 = row_lookup->y2[row];
    q = row_lookup->q[row];
    inv_q = row_lookup->inv_q[row];

    src_stride = data->src_stride;
    src = data->src;
    src_y1_buf = &src[y1 * src_stride];
    src_y2_buf = &src[y2 * src_stride];

    dst_stride = data->dst_stride;
    dst = &data->dst[row * dst_stride];

    for (col = 0; col < d_width; ++col) {
        x1_off = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x1[col]);
        x2_off = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x2[col]);
        p = col_lookup->p[col];
        inv_p = col_lookup->inv_p[col];

        w00 = sgl_q11_mul(inv_p, inv_q); /* Q11 */
        w01 = sgl_q11_mul(    p, inv_q); /* Q11 */
        w10 = sgl_q11_mul(inv_p,     q); /* Q11 */
        w11 = sgl_q11_mul(    p,     q); /* Q11 */

        src_y1x1 = &src_y1_buf[x1_off];
        src_y1x2 = &src_y1_buf[x2_off];
        src_y2x1 = &src_y2_buf[x1_off];
        src_y2x2 = &src_y2_buf[x2_off];

        pair = sgl_generic_bilinear_interpolate_pair(
            w00, w01, w10, w11, src_y1x1, src_y1x2, src_y2x1, src_y2x2);
        sgl_generic_bilinear_store_pair(dst, pair);
        pair = sgl_generic_bilinear_interpolate_pair(
            w00, w01, w10, w11, &src_y1x1[2], &src_y1x2[2],
            &src_y2x1[2], &src_y2x2[2]);
        sgl_generic_bilinear_store_pair(&dst[2], pair);

        dst = &dst[SGL_BPP32];
    }
}

/*
 * Design and Operation
 * --------------------
 * Match the NEON bpp32 fast path structure in generic C:
 *
 *   source row y ---- horizontal Q11 row cache ----+
 *                                                   +-- vertical mix
 *   source row y+1 -- horizontal Q11 row cache ----+
 *
 * A two-row scratch cache avoids rebuilding the same horizontally filtered
 * source row for adjacent destination rows.  The cache is owned by one row
 * range, so threaded workers reuse local rows without sharing state.
 */
static SGL_ALWAYS_INLINE sgl_uint8_t sgl_generic_bilinear_separable_acc_to_u8(
    sgl_q11_ext_t acc)
{
    sgl_q11_ext_t value;

    /* The rounded accumulator is nonnegative, so shift equals division by Q22. */
    value = (acc + SGL_GENERIC_BILINEAR_SEPARABLE_HALF) >>
            SGL_GENERIC_BILINEAR_SEPARABLE_BITS;

    return (sgl_uint8_t)value;
}

static SGL_ALWAYS_INLINE void sgl_generic_bilinear_horizontal_bpp32(
    const sgl_uint8_t *SGL_RESTRICT src_row,
    sgl_q11_ext_t *SGL_RESTRICT dst_row,
    const bilinear_column_lookup_t *SGL_RESTRICT col_lookup,
    sgl_int32_t d_width)
{
    sgl_int32_t col;
    sgl_int32_t dst_off;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_q11_ext_t p;
    sgl_q11_ext_t src0;
    sgl_q11_ext_t src1;

    for (col = 0; col < d_width; ++col) {
        dst_off = SGL_RESIZE_BPP32_BYTE_OFFSET(col);
        x1_off = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x1[col]);
        x2_off = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x2[col]);
        p = (sgl_q11_ext_t)col_lookup->p[col];

        src0 = (sgl_q11_ext_t)src_row[x1_off];
        src1 = (sgl_q11_ext_t)src_row[x2_off];
        dst_row[dst_off] =
            sgl_generic_bilinear_scale_nonnegative_q11(src0) +
            ((src1 - src0) * p);

        src0 = (sgl_q11_ext_t)src_row[x1_off + 1];
        src1 = (sgl_q11_ext_t)src_row[x2_off + 1];
        dst_row[dst_off + 1] =
            sgl_generic_bilinear_scale_nonnegative_q11(src0) +
            ((src1 - src0) * p);

        src0 = (sgl_q11_ext_t)src_row[x1_off + 2];
        src1 = (sgl_q11_ext_t)src_row[x2_off + 2];
        dst_row[dst_off + 2] =
            sgl_generic_bilinear_scale_nonnegative_q11(src0) +
            ((src1 - src0) * p);

        src0 = (sgl_q11_ext_t)src_row[x1_off + 3];
        src1 = (sgl_q11_ext_t)src_row[x2_off + 3];
        dst_row[dst_off + 3] =
            sgl_generic_bilinear_scale_nonnegative_q11(src0) +
            ((src1 - src0) * p);
    }
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t *sgl_generic_bilinear_get_cached_row_bpp32(
    sgl_generic_bilinear_row_cache_t *SGL_RESTRICT cache,
    sgl_int32_t y,
    const sgl_bilinear_data_t *SGL_RESTRICT data)
{
    sgl_generic_bilinear_row_cache_t *slot;
    const sgl_uint8_t *src_row;
    sgl_int32_t d_width;

    d_width = data->lut->d_width;
    slot = &cache[(sgl_uint32_t)y &
                  (sgl_uint32_t)SGL_GENERIC_BILINEAR_ROW_CACHE_MASK];
    if (slot->y != y) {
        src_row = &data->src[y * data->src_stride];
        sgl_generic_bilinear_horizontal_bpp32(
            src_row,
            slot->row,
            &data->lut->col_lookup,
            d_width);
        slot->y = y;
    }

    return slot->row;
}

static SGL_ALWAYS_INLINE void sgl_generic_bilinear_vertical_bpp32(
    sgl_uint8_t *SGL_RESTRICT dst_row,
    const sgl_q11_ext_t *SGL_RESTRICT top_row,
    const sgl_q11_ext_t *SGL_RESTRICT bottom_row,
    sgl_q11_ext_t q,
    sgl_int32_t row_width)
{
    sgl_int32_t off;
    sgl_q11_ext_t top;
    sgl_q11_ext_t bottom;

    for (off = 0; off < row_width; ++off) {
        top = top_row[off];
        bottom = bottom_row[off];
        dst_row[off] = sgl_generic_bilinear_separable_acc_to_u8(
            sgl_generic_bilinear_scale_nonnegative_q11(top) +
            ((bottom - top) * q));
    }
}

static sgl_result_t sgl_generic_resize_bilinear_range_separable_bpp32(
    sgl_bilinear_data_t *data,
    sgl_int32_t start_row,
    sgl_int32_t row_count)
{
    sgl_result_t result;
    sgl_generic_bilinear_row_cache_t
        cache[SGL_GENERIC_BILINEAR_ROW_CACHE_COUNT];
    sgl_q11_ext_t *row_storage;
    const sgl_q11_ext_t *top_row;
    const sgl_q11_ext_t *bottom_row;
    sgl_int32_t row;
    sgl_int32_t end_row;
    sgl_int32_t d_height;
    sgl_int32_t row_width;
    sgl_int32_t slot;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_uint8_t *dst_row;

    result = SGL_SUCCESS;
    row_storage = SGL_NULL;
    d_height = data->lut->d_height;
    row_width = SGL_RESIZE_BPP32_BYTE_OFFSET(data->lut->d_width);
    end_row = start_row + row_count;
    if (end_row > d_height) {
        end_row = d_height;
    }

    row_storage = sgl_memory_as_q11_ext(sgl_malloc(
        sizeof(sgl_q11_ext_t) * (sgl_size_t)row_width *
        (sgl_size_t)SGL_GENERIC_BILINEAR_ROW_CACHE_COUNT));

    if (row_storage != SGL_NULL) {
        for (slot = 0; slot < SGL_GENERIC_BILINEAR_ROW_CACHE_COUNT; ++slot) {
            cache[slot].y = -1;
            cache[slot].row = &row_storage[slot * row_width];
        }

        for (row = start_row; row < end_row; ++row) {
            y1 = data->lut->row_lookup.y1[row];
            y2 = data->lut->row_lookup.y2[row];
            top_row = sgl_generic_bilinear_get_cached_row_bpp32(
                cache, y1, data);
            bottom_row = sgl_generic_bilinear_get_cached_row_bpp32(
                cache, y2, data);
            dst_row = &data->dst[row * data->dst_stride];
            sgl_generic_bilinear_vertical_bpp32(
                dst_row,
                top_row,
                bottom_row,
                (sgl_q11_ext_t)data->lut->row_lookup.q[row],
                row_width);
        }
    }
    else {
        result = SGL_ERROR_MEMORY_ALLOCATION;
    }

    SGL_SAFE_FREE(row_storage);

    return result;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_line_stripe_scalar(
    sgl_int32_t row, sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t d_width;
    sgl_int32_t bpp;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_q11_t p;
    sgl_q11_t inv_p;
    sgl_q11_t q;
    sgl_q11_t inv_q;
    sgl_q11_t w00;
    sgl_q11_t w01;
    sgl_q11_t w10;
    sgl_q11_t w11;
    sgl_q11_ext_t acc;
    sgl_q11_ext_t value;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t ch;
    sgl_int32_t src_stride;
    sgl_int32_t dst_stride;
    const sgl_uint8_t *src_y1_buf;
    const sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y1x1;
    const sgl_uint8_t *src_y1x2;
    const sgl_uint8_t *src_y2x1;
    const sgl_uint8_t *src_y2x2;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    d_width = data->lut->d_width;
    bpp = data->bpp;

    /* set 'row' data */
    y1 = row_lookup->y1[row];
    y2 = row_lookup->y2[row];
    q = row_lookup->q[row];
    inv_q = row_lookup->inv_q[row];

    src_stride = data->src_stride;
    src = data->src;
    src_y1_buf = &src[y1 * src_stride];
    src_y2_buf = &src[y2 * src_stride];

    dst_stride = data->dst_stride;
    dst = &data->dst[row * dst_stride];

    for (col = 0; col < d_width; ++col) {
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        p = col_lookup->p[col];
        inv_p = col_lookup->inv_p[col];

        w00 = sgl_q11_mul(inv_p, inv_q); /* Q11 */
        w01 = sgl_q11_mul(    p, inv_q); /* Q11 */
        w10 = sgl_q11_mul(inv_p,     q); /* Q11 */
        w11 = sgl_q11_mul(    p,     q); /* Q11 */

        src_y1x1 = &src_y1_buf[x1_off];
        src_y1x2 = &src_y1_buf[x2_off];
        src_y2x1 = &src_y2_buf[x1_off];
        src_y2x2 = &src_y2_buf[x2_off];

        for (ch = 0; ch < bpp; ++ch) {
            acc =   ((sgl_q11_ext_t)w00 * (sgl_q11_ext_t)src_y1x1[ch]) +
                    ((sgl_q11_ext_t)w01 * (sgl_q11_ext_t)src_y1x2[ch]) +
                    ((sgl_q11_ext_t)w10 * (sgl_q11_ext_t)src_y2x1[ch]) +
                    ((sgl_q11_ext_t)w11 * (sgl_q11_ext_t)src_y2x2[ch]);
            value = sgl_q11_shift_down(sgl_q11_round_up(acc));

            /* Q11 -> u8 */
            dst[ch] = sgl_clamp_u8_i32(value);
        }
        dst = &dst[bpp];
    }
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_line_stripe(
    sgl_int32_t row, sgl_bilinear_data_t *data)
{
    switch (data->bpp) {
    case SGL_BPP32:
        sgl_generic_resize_bilinear_line_stripe_bpp32(row, data);
        break;
    default:
        sgl_generic_resize_bilinear_line_stripe_scalar(row, data);
        break;
    }
}

static sgl_int32_t sgl_generic_resize_bilinear_count_errors(
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

static SGL_ALWAYS_INLINE sgl_bool_t sgl_generic_resize_bilinear_is_same_size(
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_bool_t result;

    result = SGL_FALSE;
    if ((d_width == s_width) && (d_height == s_height)) {
        result = SGL_TRUE;
    }

    return result;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_copy_same_size(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t bpp)
{
    sgl_size_t copy_size;

    copy_size = (sgl_size_t)d_width * (sgl_size_t)d_height * (sgl_size_t)bpp;
    if (dst != src) {
        (void)sgl_memcpy(dst, src, copy_size);
    }
}

static sgl_bilinear_lookup_t *sgl_generic_resize_bilinear_select_lut(
    sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
    sgl_bilinear_lookup_t **temp_lut,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_bilinear_lookup_t *lut;

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
        *temp_lut = sgl_generic_create_bilinear_lut(
            d_width, d_height, s_width, s_height);
        lut = *temp_lut;
    }

    return lut;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_set_data(
    sgl_bilinear_data_t *data,
    sgl_bilinear_lookup_t *lut,
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

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_single_fallback(
    sgl_bilinear_data_t *data,
    sgl_int32_t d_height)
{
    sgl_int32_t row;

    for (row = 0; row < d_height; ++row) {
        sgl_generic_resize_bilinear_line_stripe(row, data);
    }
}

static sgl_result_t sgl_generic_resize_bilinear_single(
    sgl_bilinear_data_t *data,
    sgl_int32_t d_height,
    sgl_int32_t bpp)
{
    sgl_result_t result;

    result = SGL_SUCCESS;
    switch (bpp) {
    case SGL_BPP32:
        result = sgl_generic_resize_bilinear_range_separable_bpp32(
            data, 0, d_height);
        if (result != SGL_SUCCESS) {
            result = SGL_SUCCESS;
            sgl_generic_resize_bilinear_single_fallback(data, d_height);
        }
        break;
    default:
        sgl_generic_resize_bilinear_single_fallback(data, d_height);
        break;
    }

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static sgl_int32_t sgl_generic_resize_bilinear_thread_bulk_size(
    const sgl_threadpool_t *pool,
    sgl_int32_t d_height,
    sgl_int32_t bpp)
{
    sgl_int32_t bulk_size;

    switch (bpp) {
    case SGL_BPP32:
        bulk_size = SGL_GENERIC_BILINEAR_CACHE_BULK_SIZE;
        break;
    default:
        bulk_size = SGL_GENERIC_BULK_SIZE;
        break;
    }

    bulk_size = sgl_resize_thread_bulk_size(pool, d_height, bulk_size);

    return bulk_size;
}

static sgl_result_t sgl_generic_resize_bilinear_threaded(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bilinear_data_t *data,
    sgl_int32_t d_height,
    sgl_int32_t bpp)
{
    sgl_result_t result;
    sgl_bilinear_current_t *currents;
    sgl_queue_t *operations;
    sgl_int32_t i;
    sgl_int32_t num_operations;
    sgl_int32_t mod_operations;
    sgl_int32_t bulk_size;

    result = SGL_ERROR_MEMORY_ALLOCATION;
    currents = SGL_NULL;
    operations = SGL_NULL;
    bulk_size = sgl_generic_resize_bilinear_thread_bulk_size(
        pool, d_height, bpp);
    num_operations = d_height / bulk_size;
    mod_operations = d_height % bulk_size;
    if (mod_operations != 0) {
        num_operations += 1;
    }

    operations = sgl_queue_create((sgl_size_t)num_operations);
    currents = sgl_memory_as_bilinear_current(sgl_malloc(
        sizeof(sgl_bilinear_current_t) * (sgl_size_t)num_operations));
    if ((operations != SGL_NULL) && (currents != SGL_NULL)) {
        for (i = 0; i < num_operations; ++i) {
            currents[i].row = i * bulk_size;
            currents[i].count = bulk_size;
            (void)sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
        }

        if (mod_operations != 0) {
            currents[num_operations - 1].count = mod_operations;
        }

        /* multi-threaded resize */
        result = sgl_threadpool_attach_routine_consuming(
            pool,
            sgl_generic_resize_bilinear_routine,
            operations,
            (void *)data);
        sgl_queue_destroy(&operations);
    }
    SGL_SAFE_FREE(currents);
    SGL_SAFE_FREE(operations);

    return result;
}
#endif  /* !SGL_CFG_HAS_THREAD */

static sgl_result_t sgl_generic_resize_bilinear_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_bilinear_data_t *data,
    sgl_int32_t d_height,
    sgl_int32_t bpp)
{
    sgl_result_t result;

    if (pool == SGL_NULL) {
        result = sgl_generic_resize_bilinear_single(data, d_height, bpp);
    }
#if defined(SGL_CFG_HAS_THREAD)
    else {
        result = sgl_generic_resize_bilinear_threaded(pool, data, d_height, bpp);
    }
#else
    else {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
#endif  /* !SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_generic_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_bilinear_data_t data;
    sgl_bilinear_lookup_t *lut = SGL_NULL;
    sgl_bilinear_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;

    SGL_TRACE_RESIZE_BEGIN(
        SGL_TRACE_BACKEND_GENERIC,
        SGL_TRACE_METHOD_BILINEAR,
        d_width,
        d_height,
        s_width,
        s_height,
        bpp,
        SGL_TRACE_REQUESTED_THREADS(pool),
        (ext_lut != SGL_NULL));
    errcnt = sgl_generic_resize_bilinear_count_errors(
        dst, d_width, d_height, src, s_width, s_height, bpp);

    /* check error count */
    if (errcnt != 0) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }
    else if (sgl_generic_resize_bilinear_is_same_size(
                 d_width, d_height, s_width, s_height) == SGL_TRUE) {
        sgl_generic_resize_bilinear_copy_same_size(dst, d_width, d_height, src, bpp);
    }
    else {
        lut = sgl_generic_resize_bilinear_select_lut(
            ext_lut, &temp_lut, d_width, d_height, s_width, s_height);
        if (lut != SGL_NULL) {
            sgl_generic_resize_bilinear_set_data(
                &data, lut, dst, src, s_width, d_width, bpp);
            result = sgl_generic_resize_bilinear_run(pool, &data, d_height, bpp);

            if (temp_lut != SGL_NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_bilinear_lut(temp_lut);
            }
        }
    }

    SGL_TRACE_RESIZE_END(
        SGL_TRACE_BACKEND_GENERIC, SGL_TRACE_METHOD_BILINEAR, result);

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_bilinear_current_t *cur = sgl_memory_as_const_bilinear_current(current);
    sgl_bilinear_data_t *data = sgl_memory_as_bilinear_data(cookie);
    sgl_result_t result;
    sgl_int32_t row;

    result = SGL_ERROR_NOT_SUPPORTED;
    switch (data->bpp) {
    case SGL_BPP32:
        result = sgl_generic_resize_bilinear_range_separable_bpp32(
            data, cur->row, cur->count);
        break;
    default:
        break;
    }

    if (result != SGL_SUCCESS) {
        for (row = cur->row; row < (cur->row + cur->count); ++row) {
            sgl_generic_resize_bilinear_line_stripe(row, data);
        }
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
