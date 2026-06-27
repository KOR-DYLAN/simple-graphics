/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
/* SGL-CALLBACK-DEV-001: thread callbacks recover typed context from void *. */
/* cppcheck-suppress-file misra-c2012-11.5 */
/* cppcheck-suppress-file constParameterCallback */
#include <arm_neon.h>
#include <sgl-core.h>
#include "bilinear.h"

#define NEON_LANE_SIZE  (8)

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

/**
 * Optimized Bilinear Interpolation
 * Minimizes type promotion by using vmlal_s16 (Multiply-Accumulate Long).
 */
static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_bilinear_interpolation(
    uint16x8_t src_y1x1, uint16x8_t src_y1x2,
    uint16x8_t src_y2x1, uint16x8_t src_y2x2,
    sgl_simd_q11_t w00, sgl_simd_q11_t w01, sgl_simd_q11_t w10, sgl_simd_q11_t w11)
{
    int32x4_t acc_lo;
    int32x4_t acc_hi;
    int16x4_t src_y1x1_lo;
    int16x4_t src_y1x2_lo;
    int16x4_t src_y2x1_lo;
    int16x4_t src_y2x2_lo;
    int16x4_t src_y1x1_hi;
    int16x4_t src_y1x2_hi;
    int16x4_t src_y2x1_hi;
    int16x4_t src_y2x2_hi;

    /*
     * Source lanes come from uint8 pixels widened to uint16.  The interpolation
     * weights are signed Q11 values, and AArch64 provides a signed widening
     * multiply-accumulate here.  Pixel lanes are only 0..255, so reinterpreting
     * their low 16 bits as signed values preserves the numeric value while
     * satisfying the intrinsic type contract.
     */
    src_y1x1_lo = vreinterpret_s16_u16(vget_low_u16(src_y1x1));
    src_y1x2_lo = vreinterpret_s16_u16(vget_low_u16(src_y1x2));
    src_y2x1_lo = vreinterpret_s16_u16(vget_low_u16(src_y2x1));
    src_y2x2_lo = vreinterpret_s16_u16(vget_low_u16(src_y2x2));
    src_y1x1_hi = vreinterpret_s16_u16(vget_high_u16(src_y1x1));
    src_y1x2_hi = vreinterpret_s16_u16(vget_high_u16(src_y1x2));
    src_y2x1_hi = vreinterpret_s16_u16(vget_high_u16(src_y2x1));
    src_y2x2_hi = vreinterpret_s16_u16(vget_high_u16(src_y2x2));

    /* Process Low Lanes (0-3) */
    acc_lo = vmull_s16(vget_low_s16(w00), src_y1x1_lo);
    acc_lo = vmlal_s16(acc_lo, vget_low_s16(w01), src_y1x2_lo);
    acc_lo = vmlal_s16(acc_lo, vget_low_s16(w10), src_y2x1_lo);
    acc_lo = vmlal_s16(acc_lo, vget_low_s16(w11), src_y2x2_lo);

    /* Process High Lanes (4-7) */
    acc_hi = vmull_s16(vget_high_s16(w00), src_y1x1_hi);
    acc_hi = vmlal_s16(acc_hi, vget_high_s16(w01), src_y1x2_hi);
    acc_hi = vmlal_s16(acc_hi, vget_high_s16(w10), src_y2x1_hi);
    acc_hi = vmlal_s16(acc_hi, vget_high_s16(w11), src_y2x2_hi);

    /* Apply Rounding and Shift Right (Q11 -> Integer) */
    acc_lo = vrshrq_n_s32(acc_lo, SGL_Q11_FRAC_BITS);
    acc_hi = vrshrq_n_s32(acc_hi, SGL_Q11_FRAC_BITS);

    /* Final Clamping to uint8 */
    return sgl_simd_clamp_u8_i32(acc_lo, acc_hi);
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bilinear_upscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_lanes, sgl_int32_t step, sgl_int32_t bpp,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t lane;
    sgl_int32_t i;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    sgl_uint8_t *dst;

    SGL_ALIGNED(16) sgl_uint8_t x1_col[NEON_LANE_SIZE];
    SGL_ALIGNED(16) sgl_uint8_t x2_col[NEON_LANE_SIZE];

    sgl_simd_q11_t vec_p;
    sgl_simd_q11_t vec_p_inv;
    sgl_simd_q11_t vec_q;
    sgl_simd_q11_t vec_q_inv;

    sgl_simd_q11_t vec_w00;
    sgl_simd_q11_t vec_w01;
    sgl_simd_q11_t vec_w10;
    sgl_simd_q11_t vec_w11;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1;

    sgl_int32_t x1_col_base;
    sgl_int32_t x2_col_base;
    uint8x16x4_t vtbl4_src_y1x1;
    uint8x16x4_t vtbl4_src_y1x2;
    uint8x16x4_t vtbl4_src_y2x1;
    uint8x16x4_t vtbl4_src_y2x2;
    uint8x16x3_t vtbl3_src_y1x1;
    uint8x16x3_t vtbl3_src_y1x2;
    uint8x16x3_t vtbl3_src_y2x1;
    uint8x16x3_t vtbl3_src_y2x2;
    uint8x16x2_t vtbl2_src_y1x1;
    uint8x16x2_t vtbl2_src_y1x2;
    uint8x16x2_t vtbl2_src_y2x1;
    uint8x16x2_t vtbl2_src_y2x2;
    uint8x16_t vtbl1_src_y1x1;
    uint8x16_t vtbl1_src_y1x2;
    uint8x16_t vtbl1_src_y2x1;
    uint8x16_t vtbl1_src_y2x2;
    uint8x8_t vec_src_y1x1;
    uint8x8_t vec_src_y1x2;
    uint8x8_t vec_src_y2x1;
    uint8x8_t vec_src_y2x2;
    uint8x8_t vec_x1_col;
    uint8x8_t vec_x2_col;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;

    /* set 'row' data */
    vec_q = vdupq_n_s16(row_lookup->q[row]);
    vec_q_inv = vdupq_n_s16(row_lookup->inv_q[row]);

    src_y1_buf = &data->src[row_lookup->y1[row] * data->src_stride];
    src_y2_buf = &data->src[row_lookup->y2[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

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
            x1_col[i] = sgl_clamp_u8_i32(col_lookup->x1[col] - x1_col_base);
            x2_col[i] = sgl_clamp_u8_i32(col_lookup->x2[col] - x2_col_base);
            col++;
        }

        col = (lane * NEON_LANE_SIZE);
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;

        vec_x1_col = vld1_u8(x1_col);
        vec_x2_col = vld1_u8(x2_col);

        switch (bpp) {
        case 4:
            vtbl4_src_y1x1 = vld4q_u8(&src_y1_buf[x1_off]);
            vtbl4_src_y1x2 = vld4q_u8(&src_y1_buf[x2_off]);
            vtbl4_src_y2x1 = vld4q_u8(&src_y2_buf[x1_off]);
            vtbl4_src_y2x2 = vld4q_u8(&src_y2_buf[x2_off]);

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
            vtbl3_src_y1x1 = vld3q_u8(&src_y1_buf[x1_off]);
            vtbl3_src_y1x2 = vld3q_u8(&src_y1_buf[x2_off]);
            vtbl3_src_y2x1 = vld3q_u8(&src_y2_buf[x1_off]);
            vtbl3_src_y2x2 = vld3q_u8(&src_y2_buf[x2_off]);

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
            vtbl2_src_y1x1 = vld2q_u8(&src_y1_buf[x1_off]);
            vtbl2_src_y1x2 = vld2q_u8(&src_y1_buf[x2_off]);
            vtbl2_src_y2x1 = vld2q_u8(&src_y2_buf[x1_off]);
            vtbl2_src_y2x2 = vld2q_u8(&src_y2_buf[x2_off]);

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
            vtbl1_src_y1x1 = vld1q_u8(&src_y1_buf[x1_off]);
            vtbl1_src_y1x2 = vld1q_u8(&src_y1_buf[x2_off]);
            vtbl1_src_y2x1 = vld1q_u8(&src_y2_buf[x1_off]);
            vtbl1_src_y2x2 = vld1q_u8(&src_y2_buf[x2_off]);

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

        dst = &dst[step];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bilinear_downscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_lanes, sgl_int32_t step, sgl_int32_t bpp,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t lane;
    sgl_int32_t i;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y1x1;
    const sgl_uint8_t *src_y1x2;
    const sgl_uint8_t *src_y2x1;
    const sgl_uint8_t *src_y2x2;
    sgl_uint8_t *dst;

    SGL_ALIGNED(16) sgl_uint8_t serialized_src_y1x1[SGL_BPP32][NEON_LANE_SIZE];
    SGL_ALIGNED(16) sgl_uint8_t serialized_src_y1x2[SGL_BPP32][NEON_LANE_SIZE];
    SGL_ALIGNED(16) sgl_uint8_t serialized_src_y2x1[SGL_BPP32][NEON_LANE_SIZE];
    SGL_ALIGNED(16) sgl_uint8_t serialized_src_y2x2[SGL_BPP32][NEON_LANE_SIZE];

    sgl_simd_q11_t vec_p;
    sgl_simd_q11_t vec_p_inv;
    sgl_simd_q11_t vec_q;
    sgl_simd_q11_t vec_q_inv;

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

    src_y1_buf = &data->src[row_lookup->y1[row] * data->src_stride];
    src_y2_buf = &data->src[row_lookup->y2[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

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

            src_y1x1 = &src_y1_buf[x1_off];
            src_y1x2 = &src_y1_buf[x2_off];
            src_y2x1 = &src_y2_buf[x1_off];
            src_y2x2 = &src_y2_buf[x2_off];

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

        dst = &dst[step];
    }

    return dst;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_bilinear_line_stripe(sgl_int32_t row, sgl_bilinear_data_t *data) {
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t d_width;
    sgl_int32_t bpp;
    sgl_int32_t step;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y1x1;
    const sgl_uint8_t *src_y1x2;
    const sgl_uint8_t *src_y2x1;
    const sgl_uint8_t *src_y2x2;
    sgl_uint8_t *dst;

    sgl_int32_t num_lanes;

    sgl_q11_t p;
    sgl_q11_t inv_p;
    sgl_q11_t q;
    sgl_q11_t inv_q;
    sgl_q11_t w00;
    sgl_q11_t w01;
    sgl_q11_t w10;
    sgl_q11_t w11;
    sgl_int32_t acc;
    sgl_int32_t value;

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
    src_y1_buf = &data->src[row_lookup->y1[row] * data->src_stride];
    src_y2_buf = &data->src[row_lookup->y2[row] * data->src_stride];

    for (col = num_lanes * NEON_LANE_SIZE; col < d_width; ++col) {
        p = col_lookup->p[col];
        inv_p = col_lookup->inv_p[col];

        w00 = sgl_q11_mul(inv_p, inv_q);    /* Q11 */
        w01 = sgl_q11_mul(    p, inv_q);    /* Q11 */
        w10 = sgl_q11_mul(inv_p,     q);    /* Q11 */
        w11 = sgl_q11_mul(    p,     q);    /* Q11 */

        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;

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

sgl_result_t sgl_simd_resize_bilinear(
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
                    sgl_simd_resize_bilinear_line_stripe(row, (void *)&data);
                }
            }
#if defined(SGL_CFG_HAS_THREAD)
            else {
                sgl_bilinear_current_t *currents;
                sgl_queue_t *operations = SGL_NULL;
                sgl_int32_t i;
                sgl_int32_t num_operations;
                sgl_int32_t mod_operations;

                num_operations = d_height / SGL_SIMD_BULK_SIZE;
                mod_operations = d_height % SGL_SIMD_BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((sgl_size_t)num_operations);
                /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
                /* cppcheck-suppress misra-c2012-11.5 */
                currents = (sgl_bilinear_current_t *)sgl_malloc(sizeof(sgl_bilinear_current_t) * (sgl_size_t)num_operations);
                if ((operations != SGL_NULL) && (currents != SGL_NULL)) {
                    for (i = 0; i < num_operations; ++i) {
                        currents[i].row = i * SGL_SIMD_BULK_SIZE;
                        currents[i].count = SGL_SIMD_BULK_SIZE;
                        (void)sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
                    }

                    if (mod_operations != 0) {
                        currents[num_operations - 1].count = mod_operations;
                    }

                    /* multi-threaded resize */
                    (void)sgl_threadpool_attach_routine(pool, sgl_simd_resize_bilinear_routine, operations, (void *)&data);
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
static void sgl_simd_resize_bilinear_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_bilinear_current_t *cur = (const sgl_bilinear_current_t *)current;
    sgl_bilinear_data_t *data = (sgl_bilinear_data_t *)cookie;
    sgl_int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_simd_resize_bilinear_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
