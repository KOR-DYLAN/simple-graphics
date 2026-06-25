/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
/* SGL-CALLBACK-DEV-001: thread callbacks recover typed context from void *. */
/* cppcheck-suppress-file misra-c2012-11.5 */
/* cppcheck-suppress-file constParameterCallback */
#include <sgl-core.h>
#include "bilinear.h"

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE void sgl_generic_resize_bilinear_line_stripe(sgl_int32_t row, sgl_bilinear_data_t *data) {
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

sgl_result_t sgl_generic_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_int32_t row;
    sgl_bilinear_data_t data;
    sgl_bilinear_lookup_t *lut = SGL_NULL;
    sgl_bilinear_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;

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

    /* check error count */
    if (errcnt == 0) {
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
            temp_lut = sgl_generic_create_bilinear_lut(d_width, d_height, s_width, s_height);
            lut = temp_lut;
        }

        if (lut != SGL_NULL) {
            /* set data */
            data.bpp = bpp;
            data.src = src;
            data.dst = dst;
            data.lut = lut;
            data.src_stride = s_width * bpp;
            data.dst_stride = d_width * bpp;

            if (pool == SGL_NULL) {
                /* single-threaded resize */
                for (row = 0; row < d_height; ++row) {
                    sgl_generic_resize_bilinear_line_stripe(row, (void *)&data);
                }
            }
#if defined(SGL_CFG_HAS_THREAD)
            else {
                sgl_bilinear_current_t *currents;
                sgl_queue_t *operations = SGL_NULL;
                sgl_int32_t i;
                sgl_int32_t num_operations;
                sgl_int32_t mod_operations;

                num_operations = d_height / SGL_GENERIC_BULK_SIZE;
                mod_operations = d_height % SGL_GENERIC_BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((sgl_size_t)num_operations);
                /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
                /* cppcheck-suppress misra-c2012-11.5 */
                currents = (sgl_bilinear_current_t *)sgl_malloc(sizeof(sgl_bilinear_current_t) * (sgl_size_t)num_operations);
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
                    (void)sgl_threadpool_attach_routine(pool, sgl_generic_resize_bilinear_routine, operations, (void *)&data);
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

            if (temp_lut != SGL_NULL) {
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

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_bilinear_current_t *cur = (const sgl_bilinear_current_t *)current;
    sgl_bilinear_data_t *data = (sgl_bilinear_data_t *)cookie;
    sgl_int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_generic_resize_bilinear_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
