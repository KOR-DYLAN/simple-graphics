#include <stdint.h>
#include <stdlib.h>
#include <arm_neon.h>
#include "sgl.h"
#include "bilinear.h"

#define NEON_LANE_SIZE  8
#define WORD_SIZE       4
#define BULK_SIZE       4

static void sgl_simd_resize_bilinear_routine(void *current, void *cookie);

static ALWAYS_INLINE uint8x8_t sgl_neon_bilinear_interpolation(
    uint8x8_t src_y1x1, uint8x8_t src_y1x2,
    uint8x8_t src_y2x1, uint8x8_t src_y2x2,
    sgl_simd_q15_t w00_lo, sgl_simd_q15_t w01_lo, sgl_simd_q15_t w10_lo, sgl_simd_q15_t w11_lo,
    sgl_simd_q15_t w00_hi, sgl_simd_q15_t w01_hi, sgl_simd_q15_t w10_hi, sgl_simd_q15_t w11_hi)
{
    uint16x8_t temp;
    int32x4_t src_y1x1_lo, src_y1x1_hi;
    int32x4_t src_y1x2_lo, src_y1x2_hi;
    int32x4_t src_y2x1_lo, src_y2x1_hi;
    int32x4_t src_y2x2_lo, src_y2x2_hi;
    int32x4_t acc_lo, acc_hi;

    temp = vmovl_u8(src_y1x1);
    src_y1x1_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(temp)));
    src_y1x1_hi = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(temp)));

    temp = vmovl_u8(src_y1x2);
    src_y1x2_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(temp)));
    src_y1x2_hi = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(temp)));

    temp = vmovl_u8(src_y2x1);
    src_y2x1_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(temp)));
    src_y2x1_hi = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(temp)));

    temp = vmovl_u8(src_y2x2);
    src_y2x2_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(temp)));
    src_y2x2_hi = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(temp)));

    acc_lo = vmulq_s32(w00_lo, src_y1x1_lo);
    acc_lo = vmlaq_s32(acc_lo, w01_lo, src_y1x2_lo);
    acc_lo = vmlaq_s32(acc_lo, w10_lo, src_y2x1_lo);
    acc_lo = vmlaq_s32(acc_lo, w11_lo, src_y2x2_lo);
    acc_lo = SGL_SIMD_Q15_SHIFTDOWN(SGL_SIMD_Q15_ROUNDUP(acc_lo));

    acc_hi = vmulq_s32(w00_hi, src_y1x1_hi);
    acc_hi = vmlaq_s32(acc_hi, w01_hi, src_y1x2_hi);
    acc_hi = vmlaq_s32(acc_hi, w10_hi, src_y2x1_hi);
    acc_hi = vmlaq_s32(acc_hi, w11_hi, src_y2x2_hi);
    acc_hi = SGL_SIMD_Q15_SHIFTDOWN(SGL_SIMD_Q15_ROUNDUP(acc_hi));

    return sgl_simd_clamp_u8_i32(acc_lo, acc_hi);
}

static ALWAYS_INLINE void sgl_simd_resize_bilinear_line_stripe(int32_t row, sgl_bilinear_data_t *data) {
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t col;
    int32_t col_lo, col_hi;
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

    int32x4_t vec_p_lo, vec_p_hi;
    int32x4_t vec_p_inv_lo, vec_p_inv_hi;
    int32x4_t vec_q, vec_q_inv;

    int32x4_t vec_w00_lo, vec_w00_hi;
    int32x4_t vec_w01_lo, vec_w01_hi;
    int32x4_t vec_w10_lo, vec_w10_hi;
    int32x4_t vec_w11_lo, vec_w11_hi;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1;

    sgl_q15_t p, inv_p;
    sgl_q15_t q, inv_q;
    sgl_q15_t w00, w01, w10, w11;
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
    vec_q = vdupq_n_s32(row_lookup->q[row]);
    vec_q_inv = vdupq_n_s32(row_lookup->inv_q[row]);

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
        col_lo = col;
        col_hi = col + 4;
        vec_p_lo = vld1q_s32(&col_lookup->p[col_lo]);
        vec_p_hi = vld1q_s32(&col_lookup->p[col_hi]);
        vec_p_inv_lo = vld1q_s32(&col_lookup->inv_p[col_lo]);
        vec_p_inv_hi = vld1q_s32(&col_lookup->inv_p[col_hi]);

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

        vec_w00_lo = sgl_simd_q15_mul(vec_p_inv_lo, vec_q_inv);
        vec_w00_hi = sgl_simd_q15_mul(vec_p_inv_hi, vec_q_inv);
        vec_w01_lo = sgl_simd_q15_mul(vec_p_lo, vec_q_inv);
        vec_w01_hi = sgl_simd_q15_mul(vec_p_hi, vec_q_inv);
        vec_w10_lo = sgl_simd_q15_mul(vec_p_inv_lo, vec_q);
        vec_w10_hi = sgl_simd_q15_mul(vec_p_inv_hi, vec_q);
        vec_w11_lo = sgl_simd_q15_mul(vec_p_lo, vec_q);
        vec_w11_hi = sgl_simd_q15_mul(vec_p_hi, vec_q);

        switch (bpp) {
        case 4:
            for (ch = 0; ch < 4; ++ch) {
                value4.val[ch] = sgl_neon_bilinear_interpolation(
                                    vld1_u8(serialized_src_y1x1[ch]), vld1_u8(serialized_src_y1x2[ch]),
                                    vld1_u8(serialized_src_y2x1[ch]), vld1_u8(serialized_src_y2x2[ch]),
                                    vec_w00_lo, vec_w01_lo, vec_w10_lo, vec_w11_lo,
                                    vec_w00_hi, vec_w01_hi, vec_w10_hi, vec_w11_hi);
            }
            vst4_u8(dst, value4);
            break;
        case 3:
            for (ch = 0; ch < 3; ++ch) {
                value3.val[ch] = sgl_neon_bilinear_interpolation(
                                    vld1_u8(serialized_src_y1x1[ch]), vld1_u8(serialized_src_y1x2[ch]),
                                    vld1_u8(serialized_src_y2x1[ch]), vld1_u8(serialized_src_y2x2[ch]),
                                    vec_w00_lo, vec_w01_lo, vec_w10_lo, vec_w11_lo,
                                    vec_w00_hi, vec_w01_hi, vec_w10_hi, vec_w11_hi);
            }
            vst3_u8(dst, value3);
            break;
        case 2:
            for (ch = 0; ch < 2; ++ch) {
                value2.val[ch] = sgl_neon_bilinear_interpolation(
                                    vld1_u8(serialized_src_y1x1[ch]), vld1_u8(serialized_src_y1x2[ch]),
                                    vld1_u8(serialized_src_y2x1[ch]), vld1_u8(serialized_src_y2x2[ch]),
                                    vec_w00_lo, vec_w01_lo, vec_w10_lo, vec_w11_lo,
                                    vec_w00_hi, vec_w01_hi, vec_w10_hi, vec_w11_hi);
            }
            vst2_u8(dst, value2);
            break;
        case 1:
            value1 = sgl_neon_bilinear_interpolation(
                                vld1_u8(serialized_src_y1x1[0]), vld1_u8(serialized_src_y1x2[0]),
                                vld1_u8(serialized_src_y2x1[0]), vld1_u8(serialized_src_y2x2[0]),
                                vec_w00_lo, vec_w01_lo, vec_w10_lo, vec_w11_lo,
                                vec_w00_hi, vec_w01_hi, vec_w10_hi, vec_w11_hi);
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

sgl_result_t sgl_simd_resize_bilinear(
                sgl_threadpool_t *pool, sgl_bilinear_lookup_t *ext_lut, 
                uint8_t *dst, int32_t d_width, int32_t d_height, 
                uint8_t *src, int32_t s_width, int32_t s_height, 
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

static void sgl_simd_resize_bilinear_routine(void *current, void *cookie)
{
    sgl_bilinear_current_t *cur = (sgl_bilinear_current_t *)current;
    sgl_bilinear_data_t *data = (sgl_bilinear_data_t *)cookie;
    int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_simd_resize_bilinear_line_stripe(row, data);
    }
}
