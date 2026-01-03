#include <stdint.h>
#include <stdlib.h>
#include <arm_neon.h>
#include "sgl.h"
#include "bilinear.h"

#define NEON_LANE_SIZE  (8)

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

static SGL_ALWAYS_INLINE uint8_t *sgl_simd_resize_bilinear_upscale_line_stripe(
                                    int32_t row, int32_t num_lanes, int32_t step, int32_t bpp,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t col, ch, lane, i;
    int32_t x1_off, x2_off;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *dst;

    SGL_ALIGNED(16) uint8_t x1_col[NEON_LANE_SIZE], x2_col[NEON_LANE_SIZE];

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

    int32_t x1_col_base, x2_col_base;
    uint8x16x4_t vtbl4_src_y1x1, vtbl4_src_y1x2, vtbl4_src_y2x1, vtbl4_src_y2x2;
    uint8x16x3_t vtbl3_src_y1x1, vtbl3_src_y1x2, vtbl3_src_y2x1, vtbl3_src_y2x2;
    uint8x16x2_t vtbl2_src_y1x1, vtbl2_src_y1x2, vtbl2_src_y2x1, vtbl2_src_y2x2;
    uint8x16_t vtbl1_src_y1x1, vtbl1_src_y1x2, vtbl1_src_y2x1, vtbl1_src_y2x2;
    uint8x8_t vec_src_y1x1, vec_src_y1x2, vec_src_y2x1, vec_src_y2x2;
    uint8x8_t vec_x1_col, vec_x2_col;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    
    /* set 'row' data */
    vec_q = vdupq_n_s16(row_lookup->q[row]);
    vec_q_inv = vdupq_n_s16(row_lookup->inv_q[row]);

    src_y1_buf = data->src + (row_lookup->y1[row] * data->src_stride);
    src_y2_buf = data->src + (row_lookup->y2[row] * data->src_stride);
    dst = data->dst + (row * data->dst_stride);

    for (lane = 0; lane < num_lanes; ++lane) {
        col = (lane * NEON_LANE_SIZE);
        x1_col_base = col_lookup->x1[col];
        x2_col_base = col_lookup->x2[col];
        vec_p = vld1q_s16(&col_lookup->p[col]);
        vec_p_inv = vld1q_s16(&col_lookup->inv_p[col]);

        vec_w00 = sgl_simd_q11_mul(vec_p_inv, vec_q_inv);
        vec_w01 = sgl_simd_q11_mul(vec_p, vec_q_inv);
        vec_w10 = sgl_simd_q11_mul(vec_p_inv, vec_q);
        vec_w11 = sgl_simd_q11_mul(vec_p, vec_q);

        for (i = 0; i < NEON_LANE_SIZE; ++i) {
            x1_col[i] = (uint8_t)(col_lookup->x1[col] - x1_col_base);
            x2_col[i] = (uint8_t)(col_lookup->x2[col] - x2_col_base);
            col++;
        }

        col = (lane * NEON_LANE_SIZE);
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;

        vec_x1_col = vld1_u8(x1_col);
        vec_x2_col = vld1_u8(x2_col);

        switch (bpp) {
        case 4:
            vtbl4_src_y1x1 = vld4q_u8(src_y1_buf + x1_off);
            vtbl4_src_y1x2 = vld4q_u8(src_y1_buf + x2_off);
            vtbl4_src_y2x1 = vld4q_u8(src_y2_buf + x1_off);
            vtbl4_src_y2x2 = vld4q_u8(src_y2_buf + x2_off);

            for (ch = 0; ch < 4; ++ch) {
                vec_src_y1x1 = vqtbl1_u8(vtbl4_src_y1x1.val[ch], vec_x1_col);
                vec_src_y1x2 = vqtbl1_u8(vtbl4_src_y1x2.val[ch], vec_x2_col);
                vec_src_y2x1 = vqtbl1_u8(vtbl4_src_y2x1.val[ch], vec_x1_col);
                vec_src_y2x2 = vqtbl1_u8(vtbl4_src_y2x2.val[ch], vec_x2_col);

                value4.val[ch] = sgl_neon_bilinear_interpolation(
                                    vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                                    vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                                    vec_w00, vec_w01, vec_w10, vec_w11);
            }
            vst4_u8(dst, value4);
            break;
        case 3:
            vtbl3_src_y1x1 = vld3q_u8(src_y1_buf + x1_off);
            vtbl3_src_y1x2 = vld3q_u8(src_y1_buf + x2_off);
            vtbl3_src_y2x1 = vld3q_u8(src_y2_buf + x1_off);
            vtbl3_src_y2x2 = vld3q_u8(src_y2_buf + x2_off);

            for (ch = 0; ch < 3; ++ch) {
                vec_src_y1x1 = vqtbl1_u8(vtbl3_src_y1x1.val[ch], vec_x1_col);
                vec_src_y1x2 = vqtbl1_u8(vtbl3_src_y1x2.val[ch], vec_x2_col);
                vec_src_y2x1 = vqtbl1_u8(vtbl3_src_y2x1.val[ch], vec_x1_col);
                vec_src_y2x2 = vqtbl1_u8(vtbl3_src_y2x2.val[ch], vec_x2_col);
                value3.val[ch] = sgl_neon_bilinear_interpolation(
                                    vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                                    vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                                    vec_w00, vec_w01, vec_w10, vec_w11);
            }
            vst3_u8(dst, value3);
            break;
        case 2:
            vtbl2_src_y1x1 = vld2q_u8(src_y1_buf + x1_off);
            vtbl2_src_y1x2 = vld2q_u8(src_y1_buf + x2_off);
            vtbl2_src_y2x1 = vld2q_u8(src_y2_buf + x1_off);
            vtbl2_src_y2x2 = vld2q_u8(src_y2_buf + x2_off);

            for (ch = 0; ch < 2; ++ch) {
                vec_src_y1x1 = vqtbl1_u8(vtbl2_src_y1x1.val[ch], vec_x1_col);
                vec_src_y1x2 = vqtbl1_u8(vtbl2_src_y1x2.val[ch], vec_x2_col);
                vec_src_y2x1 = vqtbl1_u8(vtbl2_src_y2x1.val[ch], vec_x1_col);
                vec_src_y2x2 = vqtbl1_u8(vtbl2_src_y2x2.val[ch], vec_x2_col);
                value2.val[ch] = sgl_neon_bilinear_interpolation(
                                    vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                                    vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                                    vec_w00, vec_w01, vec_w10, vec_w11);
            }
            vst2_u8(dst, value2);
            break;
        case 1:
            vtbl1_src_y1x1 = vld1q_u8(src_y1_buf + x1_off);
            vtbl1_src_y1x2 = vld1q_u8(src_y1_buf + x2_off);
            vtbl1_src_y2x1 = vld1q_u8(src_y2_buf + x1_off);
            vtbl1_src_y2x2 = vld1q_u8(src_y2_buf + x2_off);

            vec_src_y1x1 = vqtbl1_u8(vtbl1_src_y1x1, vec_x1_col);
            vec_src_y1x2 = vqtbl1_u8(vtbl1_src_y1x2, vec_x2_col);
            vec_src_y2x1 = vqtbl1_u8(vtbl1_src_y2x1, vec_x1_col);
            vec_src_y2x2 = vqtbl1_u8(vtbl1_src_y2x2, vec_x2_col);
            value1 = sgl_neon_bilinear_interpolation(
                                vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                                vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                                vec_w00, vec_w01, vec_w10, vec_w11);

            vst1_u8(dst, value1);
            break;
        default:
            /* Unsupported bpp */
            break;
        }

        dst += step;
    }

    return dst;
}

static SGL_ALWAYS_INLINE uint8_t *sgl_simd_resize_bilinear_downscale_line_stripe(
                                    int32_t row, int32_t num_lanes, int32_t step, int32_t bpp,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t col, ch, lane, i;
    int32_t x1_off, x2_off;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *src_y1x1, *src_y1x2;
    uint8_t *src_y2x1, *src_y2x2;
    uint8_t *dst;

    SGL_ALIGNED(16) uint8_t serialized_src_y1x1[SGL_BPP32][NEON_LANE_SIZE];
    SGL_ALIGNED(16) uint8_t serialized_src_y1x2[SGL_BPP32][NEON_LANE_SIZE];
    SGL_ALIGNED(16) uint8_t serialized_src_y2x1[SGL_BPP32][NEON_LANE_SIZE];
    SGL_ALIGNED(16) uint8_t serialized_src_y2x2[SGL_BPP32][NEON_LANE_SIZE];

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

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    
    /* set 'row' data */
    vec_q = vdupq_n_s16(row_lookup->q[row]);
    vec_q_inv = vdupq_n_s16(row_lookup->inv_q[row]);

    src_y1_buf = data->src + (row_lookup->y1[row] * data->src_stride);
    src_y2_buf = data->src + (row_lookup->y2[row] * data->src_stride);
    dst = data->dst + (row * data->dst_stride);

    for (lane = 0; lane < num_lanes; ++lane) {
        col = (lane * NEON_LANE_SIZE);
        vec_p = vld1q_s16(&col_lookup->p[col]);
        vec_p_inv = vld1q_s16(&col_lookup->inv_p[col]);

        vec_w00 = sgl_simd_q11_mul(vec_p_inv, vec_q_inv);
        vec_w01 = sgl_simd_q11_mul(vec_p, vec_q_inv);
        vec_w10 = sgl_simd_q11_mul(vec_p_inv, vec_q);
        vec_w11 = sgl_simd_q11_mul(vec_p, vec_q);

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

    return dst;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_bilinear_line_stripe(int32_t row, sgl_bilinear_data_t *data) {
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t d_width, bpp, step;
    int32_t col, ch;
    int32_t x1_off, x2_off;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *src_y1x1, *src_y1x2;
    uint8_t *src_y2x1, *src_y2x2;
    uint8_t *dst;

    int32_t num_lanes;

    sgl_q11_t p, inv_p;
    sgl_q11_t q, inv_q;
    sgl_q11_t w00, w01, w10, w11;
    int32_t acc, value;

    d_width = data->lut->d_width;
    bpp = data->bpp;
    num_lanes = d_width / NEON_LANE_SIZE;
    step = bpp * NEON_LANE_SIZE;

    if (data->src_stride <= data->dst_stride) {
        dst = sgl_simd_resize_bilinear_upscale_line_stripe(row, num_lanes, step, bpp, data);
    }
    else {
        dst = sgl_simd_resize_bilinear_downscale_line_stripe(row, num_lanes, step, bpp, data);
    }

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    
    /* set 'row' data */
    q = row_lookup->q[row];
    inv_q = row_lookup->inv_q[row];
    src_y1_buf = data->src + (row_lookup->y1[row] * data->src_stride);
    src_y2_buf = data->src + (row_lookup->y2[row] * data->src_stride);

    for (col = num_lanes * NEON_LANE_SIZE; col < d_width; ++col) {
        p = col_lookup->p[col];
        inv_p = col_lookup->inv_p[col];

        w00 = sgl_q11_mul(inv_p, inv_q);    /* Q11 */
        w01 = sgl_q11_mul(    p, inv_q);    /* Q11 */
        w10 = sgl_q11_mul(inv_p,     q);    /* Q11 */
        w11 = sgl_q11_mul(    p,     q);    /* Q11 */

        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;

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
#if SGL_CFG_HAS_THREAD
            else {
                num_operations = d_height / SGL_SIMD_BULK_SIZE;
                mod_operations = d_height % SGL_SIMD_BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((size_t)num_operations);
                currents = (sgl_bilinear_current_t *)malloc(sizeof(sgl_bilinear_current_t) * (size_t)num_operations);
                if ((operations != NULL) && (currents != NULL)) {
                    for (i = 0; i < num_operations; ++i) {
                        currents[i].row = i * SGL_SIMD_BULK_SIZE;
                        currents[i].count = SGL_SIMD_BULK_SIZE;
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
#else
            else {
                result = SGL_ERROR_NOT_SUPPORTED;
            }
#endif  /* !SGL_CFG_HAS_THREAD */

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
