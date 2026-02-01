#include <stdint.h>
#include <stdlib.h>
#include <sgl-core.h>
#include "bicubic.h"

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_generic_bicubic_interpolation(sgl_q11_ext_t v1, sgl_q11_ext_t v2, sgl_q11_ext_t v3, sgl_q11_ext_t v4, sgl_q11_ext_t d)
{
    sgl_q11_ext_t v, p1, p2, p3, p4;

    p1 = 2 * v2;
    p2 = -v1 + v3;
    p3 = (2 * v1) - (5 * v2) + (4 * v3) - v4;
    p4 = -v1 + (3 * v2) - (3 * v3) + v4;

    v = p3 + sgl_q11_ext_mul(d, p4);
    v = p2 + sgl_q11_ext_mul(d, v);
    v = p1 + sgl_q11_ext_mul(d, v);

    return v / 2;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_bicubic_line_stripe(int32_t row, sgl_bicubic_data_t *data) {
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    int32_t col; 
    int32_t d_width, bpp;
    int32_t x1_off, x2_off, x3_off, x4_off;
    int32_t y1, y2, y3, y4;
    sgl_q11_t p, q;
    sgl_q11_ext_t v1, v2, v3, v4, value;
    uint8_t *src, *dst;
    int32_t ch, src_stride, dst_stride;
    uint8_t *src_y1_buf, *src_y2_buf, *src_y3_buf, *src_y4_buf;
    uint8_t *src_y1x1, *src_y1x2, *src_y1x3, *src_y1x4;
    uint8_t *src_y2x1, *src_y2x2, *src_y2x3, *src_y2x4;
    uint8_t *src_y3x1, *src_y3x2, *src_y3x3, *src_y3x4;
    uint8_t *src_y4x1, *src_y4x2, *src_y4x3, *src_y4x4;

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
    src_y1_buf = src + (y1 * src_stride);
    src_y2_buf = src + (y2 * src_stride);
    src_y3_buf = src + (y3 * src_stride);
    src_y4_buf = src + (y4 * src_stride);

    dst_stride = data->dst_stride;
    dst = data->dst + (row * dst_stride);

    for (col = 0; col < d_width; ++col) {
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        x3_off = col_lookup->x3[col] * bpp;
        x4_off = col_lookup->x4[col] * bpp;
        p = col_lookup->p[col];

        src_y1x1 = src_y1_buf + x1_off;
        src_y1x2 = src_y1_buf + x2_off;
        src_y1x3 = src_y1_buf + x3_off;
        src_y1x4 = src_y1_buf + x4_off;

        src_y2x1 = src_y2_buf + x1_off;
        src_y2x2 = src_y2_buf + x2_off;
        src_y2x3 = src_y2_buf + x3_off;
        src_y2x4 = src_y2_buf + x4_off;

        src_y3x1 = src_y3_buf + x1_off;
        src_y3x2 = src_y3_buf + x2_off;
        src_y3x3 = src_y3_buf + x3_off;
        src_y3x4 = src_y3_buf + x4_off;

        src_y4x1 = src_y4_buf + x1_off;
        src_y4x2 = src_y4_buf + x2_off;
        src_y4x3 = src_y4_buf + x3_off;
        src_y4x4 = src_y4_buf + x4_off;

        for (ch = 0; ch < bpp; ++ch) {
            v1 = sgl_generic_bicubic_interpolation(SGL_INT_TO_Q11((sgl_q11_ext_t)src_y1x1[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y1x2[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y1x3[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y1x4[ch]), (sgl_q11_ext_t)p);
            v2 = sgl_generic_bicubic_interpolation(SGL_INT_TO_Q11((sgl_q11_ext_t)src_y2x1[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y2x2[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y2x3[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y2x4[ch]), (sgl_q11_ext_t)p);
            v3 = sgl_generic_bicubic_interpolation(SGL_INT_TO_Q11((sgl_q11_ext_t)src_y3x1[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y3x2[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y3x3[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y3x4[ch]), (sgl_q11_ext_t)p);
            v4 = sgl_generic_bicubic_interpolation(SGL_INT_TO_Q11((sgl_q11_ext_t)src_y4x1[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y4x2[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y4x3[ch]), SGL_INT_TO_Q11((sgl_q11_ext_t)src_y4x4[ch]), (sgl_q11_ext_t)p);
            value = sgl_generic_bicubic_interpolation(v1, v2, v3, v4, (sgl_q11_ext_t)q);
            value = SGL_Q11_SHIFTDOWN(SGL_Q11_ROUNDUP(value));

            /* Q11 -> u8 */
            dst[ch] = sgl_clamp_u8_i32(value);
        }
        dst += bpp;
    }
}

sgl_result_t sgl_generic_resize_bicubic(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row;
    sgl_bicubic_data_t data;
    sgl_bicubic_lookup_t *lut = NULL, *temp_lut = NULL;
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

    /* check error count */
    if (errcnt == 0) {
        if (ext_lut != NULL) {
            if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
                (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
            {
                /* apply external look-up table */
                lut = ext_lut;
            }
        }

        if (lut == NULL) {
            /* create temp look-up table */
            temp_lut = sgl_generic_create_bicubic_lut(d_width, d_height, s_width, s_height);
            lut = temp_lut;
        }

        if (lut != NULL) {
            /* set data */
            data.bpp = bpp;
            data.src = src;
            data.dst = dst;
            data.lut = lut;
            data.src_stride = s_width * bpp;
            data.dst_stride = d_width * bpp;

            if (pool == NULL) {
                /* single-threaded resize */
                for (row = 0; row < d_height; ++row) {
                    sgl_generic_resize_bicubic_line_stripe(row, (void *)&data);
                }
            }
#if defined(SGL_CFG_HAS_THREAD)
            else {
                sgl_bicubic_current_t *currents;
                sgl_queue_t *operations = NULL;
                int32_t i, num_operations, mod_operations;

                num_operations = d_height / SGL_GENERIC_BULK_SIZE;
                mod_operations = d_height % SGL_GENERIC_BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((size_t)num_operations);
                currents = (sgl_bicubic_current_t *)malloc(sizeof(sgl_bicubic_current_t) * (size_t)num_operations);
                if ((operations != NULL) && (currents != NULL)) {
                    for (i = 0; i < num_operations; ++i) {
                        currents[i].row = i * SGL_GENERIC_BULK_SIZE;
                        currents[i].count = SGL_GENERIC_BULK_SIZE;
                        sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
                    }

                    if (mod_operations != 0) {
                        currents[num_operations - 1].count = mod_operations;
                    }

                    /* multi-threaded resize */
                    sgl_threadpool_attach_routine(pool, sgl_generic_resize_bicubic_routine, operations, (void *)&data);
                    sgl_queue_destroy(&operations);
                }
                else {
                    result = SGL_ERROR_MEMORY_ALLOCATION;
                }

                SGL_SAFE_FREE(currents);
                SGL_SAFE_FREE(operations);
            }
#else
            else {
                result = SGL_ERROR_NOT_SUPPORTED;
            }
#endif  /* !SGL_CFG_HAS_THREAD */

            if (temp_lut != NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_bicubic_lut(temp_lut);
            }
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    sgl_bicubic_current_t *cur = (sgl_bicubic_current_t *)current;
    sgl_bicubic_data_t *data = (sgl_bicubic_data_t *)cookie;
    int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_generic_resize_bicubic_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
