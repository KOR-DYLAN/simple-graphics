#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"
#include "bilinear.h"

static void sgl_generic_resize_bilinear_line_stripe(void *current, void *cookie);

sgl_result_t sgl_generic_resize_bilinear(
                sgl_threadpool_t *pool, sgl_bilinear_lookup_t *ext_lut, 
                uint8_t *dst, int32_t d_width, int32_t d_height, 
                uint8_t *src, int32_t s_width, int32_t s_height, 
                int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_bilinear_current_t cur, *currents;
    sgl_bilinear_data_t data;
    sgl_queue_t *operations = NULL;
    sgl_bilinear_lookup_t *lut = NULL, *temp_lut = NULL;
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
            temp_lut = sgl_generic_create_bilinear_lut(d_width, d_height, s_width, s_height);
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
                for (cur.row = 0; cur.row < d_height; ++cur.row) {
                    sgl_generic_resize_bilinear_line_stripe((void *)&cur, (void *)&data);
                }
            }
            else {
                operations = sgl_queue_create((size_t)d_height);
                currents = (sgl_bilinear_current_t *)malloc(sizeof(sgl_bilinear_current_t) * (size_t)d_height);
                if ((operations != NULL) && (currents != NULL)) {
                    for (cur.row = 0; cur.row < d_height; ++cur.row) {
                        currents[cur.row].row = cur.row;
                        sgl_queue_unsafe_enqueue(operations, (const void *)&currents[cur.row]);
                    }

                    /* multi-threaded resize */
                    sgl_threadpool_attach_routine(pool, sgl_generic_resize_bilinear_line_stripe, operations, (void *)&data);
                    sgl_queue_destroy(&operations);
                }
                else {
                    result = SGL_ERROR_MEMORY_ALLOCATION;
                }

                SGL_SAFE_FREE(currents);
                SGL_SAFE_FREE(operations);
            }

            if (temp_lut != NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_bilinear_lut(temp_lut);
            }
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

static void sgl_generic_resize_bilinear_line_stripe(void *current, void *cookie) {
    sgl_bilinear_current_t *cur = (sgl_bilinear_current_t *)current;
    sgl_bilinear_data_t *data = (sgl_bilinear_data_t *)cookie;
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t row, col; 
    int32_t d_width, bpp;
    int32_t x1_off, x2_off;
    int32_t y1, y2;
    sgl_q15_t p, inv_p;
    sgl_q15_t q, inv_q;
    sgl_q15_t w00, w01, w10, w11;
    int32_t acc, value;
    uint8_t *src, *dst;
    int32_t ch, src_stride, dst_stride;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *src_y1x1, *src_y1x2;
    uint8_t *src_y2x1, *src_y2x2;

    /* set common data */
    row_lookup = data->lut->row_lookup;
    col_lookup = data->lut->col_lookup;
    d_width = data->lut->d_width;
    bpp = data->bpp;
    
    /* set 'row' data */
    row = cur->row;
    y1 = row_lookup[row].y1;
    y2 = row_lookup[row].y2;
    q = row_lookup[row].q;
    inv_q = row_lookup[row].inv_q;

    src_stride = data->src_stride;
    src = data->src;
    src_y1_buf = src + (y1 * src_stride);
    src_y2_buf = src + (y2 * src_stride);

    dst_stride = data->dst_stride;
    dst = data->dst + (row * dst_stride);

    for (col = 0; col < d_width; ++col) {
        x1_off = col_lookup[col].x1 * bpp;
        x2_off = col_lookup[col].x2 * bpp;
        p = col_lookup[col].p;
        inv_p = col_lookup[col].inv_p;

        w00 = sgl_q15_mul(inv_p, inv_q); /* Q15 */
        w01 = sgl_q15_mul(    p, inv_q); /* Q15 */
        w10 = sgl_q15_mul(inv_p,     q); /* Q15 */
        w11 = sgl_q15_mul(    p,     q); /* Q15 */

        src_y1x1 = src_y1_buf + x1_off;
        src_y1x2 = src_y1_buf + x2_off;
        src_y2x1 = src_y2_buf + x1_off;
        src_y2x2 = src_y2_buf + x2_off;
        for (ch = 0; ch < bpp; ++ch) {
            acc =   (w00 * src_y1x1[ch]) + 
                    (w01 * src_y1x2[ch]) +
                    (w10 * src_y2x1[ch]) + 
                    (w11 * src_y2x2[ch]);
            value = SGL_Q15_SHIFTDOWN(SGL_Q15_ROUNDUP(acc));

            /* Q15 -> u8 */
            dst[ch] = sgl_clamp_u8_i32(value);
        }
        dst += bpp;
    }
}
