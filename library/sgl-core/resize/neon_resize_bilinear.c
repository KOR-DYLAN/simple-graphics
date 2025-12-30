#include <stdint.h>
#include <stdlib.h>
#include <arm_neon.h>
#include "sgl.h"
#include "bilinear.h"

#define NEON_LANE_SIZE  8
#define WORD_SIZE       4
#define BULK_SIZE       8

static void sgl_simd_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);

/**
 * Optimized Bilinear Interpolation
 * Minimizes type promotion by using vmlal_s16 (Multiply-Accumulate Long).
 */
static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_bilinear_interpolation(
    uint16x8_t src_y1x1, uint16x8_t src_y1x2,
    uint16x8_t src_y2x1, uint16x8_t src_y2x2,
    sgl_simd_q11_t w00, sgl_simd_q11_t w01, sgl_simd_q11_t w10, sgl_simd_q11_t w11)
{
    int32x4_t acc_lo, acc_hi;

    /* Process Low Lanes (0-3) */
    acc_lo = vmull_s16(vget_low_s16(w00), vget_low_u16(src_y1x1));
    acc_lo = vmlal_s16(acc_lo, vget_low_s16(w01), vget_low_u16(src_y1x2));
    acc_lo = vmlal_s16(acc_lo, vget_low_s16(w10), vget_low_u16(src_y2x1));
    acc_lo = vmlal_s16(acc_lo, vget_low_s16(w11), vget_low_u16(src_y2x2));

    /* Process High Lanes (4-7) */
    acc_hi = vmull_s16(vget_high_s16(w00), vget_high_u16(src_y1x1));
    acc_hi = vmlal_s16(acc_hi, vget_high_s16(w01), vget_high_u16(src_y1x2));
    acc_hi = vmlal_s16(acc_hi, vget_high_s16(w10), vget_high_u16(src_y2x1));
    acc_hi = vmlal_s16(acc_hi, vget_high_s16(w11), vget_high_u16(src_y2x2));

    /* Apply Rounding and Shift Right (Q11 -> Integer) */
    acc_lo = vrshrq_n_s32(acc_lo, SGL_Q11_FRAC_BITS);
    acc_hi = vrshrq_n_s32(acc_hi, SGL_Q11_FRAC_BITS);

    /* Final Clamping to uint8 */
    return sgl_simd_clamp_u8_i32(acc_lo, acc_hi);
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_bilinear_line_stripe(int32_t row, sgl_bilinear_data_t *data) {
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t col;
    int32_t d_width, bpp, step;
    int32_t x1_off, x2_off;
    int32_t y1, y2;
    uint8_t *src, *dst;
    int32_t ch, src_stride, dst_stride;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *src_y1x1, *src_y1x2;
    uint8_t *src_y2x1, *src_y2x2;

    int32_t lane, i, num_lanes;
    uint8_t serialized_src_y1x1[WORD_SIZE][NEON_LANE_SIZE];
    uint8_t serialized_src_y1x2[WORD_SIZE][NEON_LANE_SIZE];
    uint8_t serialized_src_y2x1[WORD_SIZE][NEON_LANE_SIZE];
    uint8_t serialized_src_y2x2[WORD_SIZE][NEON_LANE_SIZE];

    sgl_simd_q11_t vec_p, vec_p_inv;
    sgl_simd_q11_t vec_q, vec_q_inv;

    sgl_simd_q11_t vec_w00;
    sgl_simd_q11_t vec_w01;
    sgl_simd_q11_t vec_w10;
    sgl_simd_q11_t vec_w11;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1;

    sgl_q11_t p, inv_p;
    sgl_q11_t q, inv_q;
    sgl_q11_t w00, w01, w10, w11;
    int32_t acc, value;

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
    vec_q = vdupq_n_s16(row_lookup->q[row]);
    vec_q_inv = vdupq_n_s16(row_lookup->inv_q[row]);

    src_stride = data->src_stride;
    src = data->src;
    src_y1_buf = src + (y1 * src_stride);
    src_y2_buf = src + (y2 * src_stride);

    dst_stride = data->dst_stride;
    dst = data->dst + (row * dst_stride);
    step = bpp * NEON_LANE_SIZE;

    /* calc lane */
    num_lanes = d_width / NEON_LANE_SIZE;

    for (lane = 0; lane < num_lanes; ++lane) {
        col = (lane * NEON_LANE_SIZE);
        vec_p = vld1q_s16(&col_lookup->p[col]);
        vec_p_inv = vld1q_s16(&col_lookup->inv_p[col]);

        for (i = 0; i < NEON_LANE_SIZE; ++i) {
            x1_off = col_lookup->x1[col] * bpp;
            x2_off = col_lookup->x2[col] * bpp;
            col++;

            src_y1x1 = src_y1_buf + x1_off;
            src_y1x2 = src_y1_buf + x2_off;
            src_y2x1 = src_y2_buf + x1_off;
            src_y2x2 = src_y2_buf + x2_off;

            for (ch = 0; ch < bpp; ++ch) {
                serialized_src_y1x1[ch][i] = src_y1x1[ch];
                serialized_src_y1x2[ch][i] = src_y1x2[ch];
                serialized_src_y2x1[ch][i] = src_y2x1[ch];
                serialized_src_y2x2[ch][i] = src_y2x2[ch];
            }
        }

        vec_w00 = sgl_simd_q11_mul(vec_p_inv, vec_q_inv);
        vec_w01 = sgl_simd_q11_mul(vec_p, vec_q_inv);
        vec_w10 = sgl_simd_q11_mul(vec_p_inv, vec_q);
        vec_w11 = sgl_simd_q11_mul(vec_p, vec_q);

        switch (bpp) {
        case 4:
            for (ch = 0; ch < 4; ++ch) {
                value4.val[ch] = sgl_neon_bilinear_interpolation(
                                    vmovl_u8(vld1_u8(serialized_src_y1x1[ch])), vmovl_u8(vld1_u8(serialized_src_y1x2[ch])),
                                    vmovl_u8(vld1_u8(serialized_src_y2x1[ch])), vmovl_u8(vld1_u8(serialized_src_y2x2[ch])),
                                    vec_w00, vec_w01, vec_w10, vec_w11);
            }
            vst4_u8(dst, value4);
            break;
        case 3:
            for (ch = 0; ch < 3; ++ch) {
                value3.val[ch] = sgl_neon_bilinear_interpolation(
                                    vmovl_u8(vld1_u8(serialized_src_y1x1[ch])), vmovl_u8(vld1_u8(serialized_src_y1x2[ch])),
                                    vmovl_u8(vld1_u8(serialized_src_y2x1[ch])), vmovl_u8(vld1_u8(serialized_src_y2x2[ch])),
                                    vec_w00, vec_w01, vec_w10, vec_w11);
            }
            vst3_u8(dst, value3);
            break;
        case 2:
            for (ch = 0; ch < 2; ++ch) {
                value2.val[ch] = sgl_neon_bilinear_interpolation(
                                    vmovl_u8(vld1_u8(serialized_src_y1x1[ch])), vmovl_u8(vld1_u8(serialized_src_y1x2[ch])),
                                    vmovl_u8(vld1_u8(serialized_src_y2x1[ch])), vmovl_u8(vld1_u8(serialized_src_y2x2[ch])),
                                    vec_w00, vec_w01, vec_w10, vec_w11);
            }
            vst2_u8(dst, value2);
            break;
        case 1:
            value1 = sgl_neon_bilinear_interpolation(
                                vmovl_u8(vld1_u8(serialized_src_y1x1[0])), vmovl_u8(vld1_u8(serialized_src_y1x2[0])),
                                vmovl_u8(vld1_u8(serialized_src_y2x1[0])), vmovl_u8(vld1_u8(serialized_src_y2x2[0])),
                                vec_w00, vec_w01, vec_w10, vec_w11);
            vst1_u8(dst, value1);
            break;
        default:
            /* Unsupported bpp */
            break;
        }
        dst += step;
    }

    for (col = num_lanes * NEON_LANE_SIZE; col < d_width; ++col) {
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        p = col_lookup->p[col];
        inv_p = col_lookup->inv_p[col];

        w00 = sgl_q11_mul(inv_p, inv_q); /* Q11 */
        w01 = sgl_q11_mul(    p, inv_q); /* Q11 */
        w10 = sgl_q11_mul(inv_p,     q); /* Q11 */
        w11 = sgl_q11_mul(    p,     q); /* Q11 */

        src_y1x1 = src_y1_buf + x1_off;
        src_y1x2 = src_y1_buf + x2_off;
        src_y2x1 = src_y2_buf + x1_off;
        src_y2x2 = src_y2_buf + x2_off;

        for (ch = 0; ch < bpp; ++ch) {
            acc =   (w00 * src_y1x1[ch]) + 
                    (w01 * src_y1x2[ch]) +
                    (w10 * src_y2x1[ch]) + 
                    (w11 * src_y2x2[ch]);
            value = SGL_Q11_SHIFTDOWN(SGL_Q11_ROUNDUP(acc));

            /* Q11 -> u8 */
            dst[ch] = sgl_clamp_u8_i32(value);
        }
        dst += bpp;
    }
}

sgl_result_t sgl_simd_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row;
    sgl_bilinear_current_t *currents;
    sgl_bilinear_data_t data;
    sgl_queue_t *operations = NULL;
    sgl_bilinear_lookup_t *lut = NULL, *temp_lut = NULL;
    int32_t i, num_operations, mod_operations;
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
                for (row = 0; row < d_height; ++row) {
                    sgl_simd_resize_bilinear_line_stripe(row, (void *)&data);
                }
            }
            else {
                num_operations = d_height / BULK_SIZE;
                mod_operations = d_height % BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((size_t)num_operations);
                currents = (sgl_bilinear_current_t *)malloc(sizeof(sgl_bilinear_current_t) * (size_t)num_operations);
                if ((operations != NULL) && (currents != NULL)) {
                    for (i = 0; i < num_operations; ++i) {
                        currents[i].row = i * BULK_SIZE;
                        currents[i].count = BULK_SIZE;
                        sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
                    }

                    if (mod_operations != 0) {
                        currents[num_operations - 1].count = mod_operations;
                    }

                    /* multi-threaded resize */
                    sgl_threadpool_attach_routine(pool, sgl_simd_resize_bilinear_routine, operations, (void *)&data);
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

static void sgl_simd_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    sgl_bilinear_current_t *cur = (sgl_bilinear_current_t *)current;
    sgl_bilinear_data_t *data = (sgl_bilinear_data_t *)cookie;
    int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_simd_resize_bilinear_line_stripe(row, data);
    }
}
