#include <stdint.h>
#include <stdlib.h>
#include "sgl-core.h"
#include "bicubic.h"

#define NEON_LANE_SIZE  (8)

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_vset_u8(uint8_t *y_buf, int32_t *x, int32_t ch, int32_t bpp)
{
    uint8x8_t vec = vdup_n_u8(0);

    vec = vset_lane_u8((y_buf + (x[0] * bpp))[ch], vec, 0);
    vec = vset_lane_u8((y_buf + (x[1] * bpp))[ch], vec, 1);
    vec = vset_lane_u8((y_buf + (x[2] * bpp))[ch], vec, 2);
    vec = vset_lane_u8((y_buf + (x[3] * bpp))[ch], vec, 3);
    vec = vset_lane_u8((y_buf + (x[4] * bpp))[ch], vec, 4);
    vec = vset_lane_u8((y_buf + (x[5] * bpp))[ch], vec, 5);
    vec = vset_lane_u8((y_buf + (x[6] * bpp))[ch], vec, 6);
    vec = vset_lane_u8((y_buf + (x[7] * bpp))[ch], vec, 7);

    return vec;
}

static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_vld_col(int32_t *lut, int32_t col_base)
{
    uint8x8_t vec = vdup_n_u8(0);

    vec = vset_lane_u8((uint8_t)(lut[0] - col_base), vec, 0);
    vec = vset_lane_u8((uint8_t)(lut[1] - col_base), vec, 1);
    vec = vset_lane_u8((uint8_t)(lut[2] - col_base), vec, 2);
    vec = vset_lane_u8((uint8_t)(lut[3] - col_base), vec, 3);
    vec = vset_lane_u8((uint8_t)(lut[4] - col_base), vec, 4);
    vec = vset_lane_u8((uint8_t)(lut[5] - col_base), vec, 5);
    vec = vset_lane_u8((uint8_t)(lut[6] - col_base), vec, 6);
    vec = vset_lane_u8((uint8_t)(lut[7] - col_base), vec, 7);

    return vec;

}

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

static SGL_ALWAYS_INLINE sgl_simd_q11_ext_t sgl_neon_bicubic_interpolation(sgl_simd_q11_ext_t v1, sgl_simd_q11_ext_t v2, sgl_simd_q11_ext_t v3, sgl_simd_q11_ext_t v4, sgl_simd_q11_ext_t d)
{
    sgl_simd_q11_ext_t v, p1, p2, p3, p4;

    p1 = vmulq_n_s32(v2, 2);
    p2 = vsubq_s32(v3, v1);
    
    p3 = vmulq_n_s32(v1, 2);
    p3 = vmlsq_n_s32(p3, v2, 5);
    p3 = vmlaq_n_s32(p3, v3, 4);
    p3 = vsubq_s32(p3, v4);

    p4 = vnegq_s32(v1);
    p4 = vmlaq_n_s32(p4, v2, 3);
    p4 = vmlsq_n_s32(p4, v3, 3);
    p4 = vaddq_s32(p4, v4);

    v = sgl_simd_q11_mul(d, p4);
    v = vaddq_s32(p3, v);

    v = sgl_simd_q11_mul(d, v);
    v = vaddq_s32(p2, v);

    v = sgl_simd_q11_mul(d, v);
    v = vaddq_s32(p1, v);

    return vshrq_n_s32(v, 1);
}

static SGL_ALWAYS_INLINE sgl_simd_q11_ext_t sgl_neon_bicubic_interpolation_lo(uint8x8_t v1, uint8x8_t v2, uint8x8_t v3, uint8x8_t v4, sgl_simd_q11_ext_t d)
{
    sgl_simd_q11_ext_t v1_lo, v2_lo, v3_lo, v4_lo;

    v1_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v1))), SGL_Q11_FRAC_BITS));
    v2_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v2))), SGL_Q11_FRAC_BITS));
    v3_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v3))), SGL_Q11_FRAC_BITS));
    v4_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v4))), SGL_Q11_FRAC_BITS));

    return sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, d);
}

static SGL_ALWAYS_INLINE sgl_simd_q11_ext_t sgl_neon_bicubic_interpolation_hi(uint8x8_t v1, uint8x8_t v2, uint8x8_t v3, uint8x8_t v4, sgl_simd_q11_ext_t d)
{
    sgl_simd_q11_ext_t v1_hi, v2_hi, v3_hi, v4_hi;

    v1_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v1)), SGL_Q11_FRAC_BITS));
    v2_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v2)), SGL_Q11_FRAC_BITS));
    v3_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v3)), SGL_Q11_FRAC_BITS));
    v4_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v4)), SGL_Q11_FRAC_BITS));

    return sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, d);

}

static SGL_ALWAYS_INLINE uint8_t *sgl_simd_resize_bicubic_upscale_line_stripe(
                                    int32_t row, int32_t num_lanes, int32_t step, int32_t bpp,
                                    sgl_bicubic_data_t *data)
{
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    int32_t col, ch, lane;
    int32_t x1_off, x2_off, x3_off, x4_off;
    uint8_t *src_y1_buf, *src_y2_buf, *src_y3_buf, *src_y4_buf;
    uint8_t *dst;

    uint8x8_t x1_col, x2_col, x3_col, x4_col;

    sgl_simd_q11_t p;
    sgl_simd_q11_ext_t p_lo, p_hi, q;

    uint8x16x4_t vtbl4_y1x1, vtbl4_y1x2, vtbl4_y1x3, vtbl4_y1x4;
    uint8x16x4_t vtbl4_y2x1, vtbl4_y2x2, vtbl4_y2x3, vtbl4_y2x4;
    uint8x16x4_t vtbl4_y3x1, vtbl4_y3x2, vtbl4_y3x3, vtbl4_y3x4;
    uint8x16x4_t vtbl4_y4x1, vtbl4_y4x2, vtbl4_y4x3, vtbl4_y4x4;

    uint8x16x3_t vtbl3_y1x1, vtbl3_y1x2, vtbl3_y1x3, vtbl3_y1x4;
    uint8x16x3_t vtbl3_y2x1, vtbl3_y2x2, vtbl3_y2x3, vtbl3_y2x4;
    uint8x16x3_t vtbl3_y3x1, vtbl3_y3x2, vtbl3_y3x3, vtbl3_y3x4;
    uint8x16x3_t vtbl3_y4x1, vtbl3_y4x2, vtbl3_y4x3, vtbl3_y4x4;

    uint8x16x2_t vtbl2_y1x1, vtbl2_y1x2, vtbl2_y1x3, vtbl2_y1x4;
    uint8x16x2_t vtbl2_y2x1, vtbl2_y2x2, vtbl2_y2x3, vtbl2_y2x4;
    uint8x16x2_t vtbl2_y3x1, vtbl2_y3x2, vtbl2_y3x3, vtbl2_y3x4;
    uint8x16x2_t vtbl2_y4x1, vtbl2_y4x2, vtbl2_y4x3, vtbl2_y4x4;

    uint8x16_t   vtbl1_y1x1, vtbl1_y1x2, vtbl1_y1x3, vtbl1_y1x4;
    uint8x16_t   vtbl1_y2x1, vtbl1_y2x2, vtbl1_y2x3, vtbl1_y2x4;
    uint8x16_t   vtbl1_y3x1, vtbl1_y3x2, vtbl1_y3x3, vtbl1_y3x4;
    uint8x16_t   vtbl1_y4x1, vtbl1_y4x2, vtbl1_y4x3, vtbl1_y4x4;

    uint8x8_t x1, x2, x3, x4;

    sgl_simd_q11_ext_t v1_lo, v2_lo, v3_lo, v4_lo;
    sgl_simd_q11_ext_t v1_hi, v2_hi, v3_hi, v4_hi;
    sgl_simd_q11_ext_t v_lo, v_hi;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1; 
    
    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    
    /* set 'row' data */
    q = vdupq_n_s32((int32_t)row_lookup->q[row]);

    src_y1_buf = data->src + (row_lookup->y1[row] * data->src_stride);
    src_y2_buf = data->src + (row_lookup->y2[row] * data->src_stride);
    src_y3_buf = data->src + (row_lookup->y3[row] * data->src_stride);
    src_y4_buf = data->src + (row_lookup->y4[row] * data->src_stride);
    dst = data->dst + (row * data->dst_stride);

    for (lane = 0; lane < num_lanes; ++lane) {
        col = (lane * NEON_LANE_SIZE);

        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        x3_off = col_lookup->x3[col] * bpp;
        x4_off = col_lookup->x4[col] * bpp;

        p = vld1q_s16(&col_lookup->p[col]);
        p_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_s16(p)));
        p_hi = vreinterpretq_s32_u32(vmovl_high_u16(p));

        x1_col = sgl_neon_vld_col(&col_lookup->x1[col], col_lookup->x1[col]);
        x2_col = sgl_neon_vld_col(&col_lookup->x2[col], col_lookup->x2[col]);
        x3_col = sgl_neon_vld_col(&col_lookup->x3[col], col_lookup->x3[col]);
        x4_col = sgl_neon_vld_col(&col_lookup->x4[col], col_lookup->x4[col]);

        switch (bpp) {
        case SGL_BPP32:
            vtbl4_y1x1 = vld4q_u8(src_y1_buf + x1_off);
            vtbl4_y1x2 = vld4q_u8(src_y1_buf + x2_off);
            vtbl4_y1x3 = vld4q_u8(src_y1_buf + x3_off);
            vtbl4_y1x4 = vld4q_u8(src_y1_buf + x4_off);

            vtbl4_y2x1 = vld4q_u8(src_y2_buf + x1_off);
            vtbl4_y2x2 = vld4q_u8(src_y2_buf + x2_off);
            vtbl4_y2x3 = vld4q_u8(src_y2_buf + x3_off);
            vtbl4_y2x4 = vld4q_u8(src_y2_buf + x4_off);

            vtbl4_y3x1 = vld4q_u8(src_y3_buf + x1_off);
            vtbl4_y3x2 = vld4q_u8(src_y3_buf + x2_off);
            vtbl4_y3x3 = vld4q_u8(src_y3_buf + x3_off);
            vtbl4_y3x4 = vld4q_u8(src_y3_buf + x4_off);

            vtbl4_y4x1 = vld4q_u8(src_y4_buf + x1_off);
            vtbl4_y4x2 = vld4q_u8(src_y4_buf + x2_off);
            vtbl4_y4x3 = vld4q_u8(src_y4_buf + x3_off);
            vtbl4_y4x4 = vld4q_u8(src_y4_buf + x4_off);

            for (ch = 0; ch < SGL_BPP32; ++ch) {
                x1 = vqtbl1_u8(vtbl4_y1x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl4_y1x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl4_y1x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl4_y1x4.val[ch], x4_col);
                v1_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v1_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl4_y2x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl4_y2x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl4_y2x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl4_y2x4.val[ch], x4_col);
                v2_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v2_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl4_y3x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl4_y3x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl4_y3x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl4_y3x4.val[ch], x4_col);
                v3_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v3_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl4_y4x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl4_y4x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl4_y4x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl4_y4x4.val[ch], x4_col);
                v4_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v4_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                v_lo = sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, q);
                v_hi = sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, q);

                /* Apply Rounding and Shift Right (Q11 -> Integer) */
                v_lo = vrshrq_n_s32(v_lo, SGL_Q11_FRAC_BITS);
                v_hi = vrshrq_n_s32(v_hi, SGL_Q11_FRAC_BITS);

                value4.val[ch] = sgl_simd_clamp_u8_i32(v_lo, v_hi);
            }

            vst4_u8(dst, value4);
            break;
        case SGL_BPP24:
            vtbl3_y1x1 = vld3q_u8(src_y1_buf + x1_off);
            vtbl3_y1x2 = vld3q_u8(src_y1_buf + x2_off);
            vtbl3_y1x3 = vld3q_u8(src_y1_buf + x3_off);
            vtbl3_y1x4 = vld3q_u8(src_y1_buf + x4_off);

            vtbl3_y2x1 = vld3q_u8(src_y2_buf + x1_off);
            vtbl3_y2x2 = vld3q_u8(src_y2_buf + x2_off);
            vtbl3_y2x3 = vld3q_u8(src_y2_buf + x3_off);
            vtbl3_y2x4 = vld3q_u8(src_y2_buf + x4_off);

            vtbl3_y3x1 = vld3q_u8(src_y3_buf + x1_off);
            vtbl3_y3x2 = vld3q_u8(src_y3_buf + x2_off);
            vtbl3_y3x3 = vld3q_u8(src_y3_buf + x3_off);
            vtbl3_y3x4 = vld3q_u8(src_y3_buf + x4_off);

            vtbl3_y4x1 = vld3q_u8(src_y4_buf + x1_off);
            vtbl3_y4x2 = vld3q_u8(src_y4_buf + x2_off);
            vtbl3_y4x3 = vld3q_u8(src_y4_buf + x3_off);
            vtbl3_y4x4 = vld3q_u8(src_y4_buf + x4_off);

            for (ch = 0; ch < SGL_BPP24; ++ch) {
                x1 = vqtbl1_u8(vtbl3_y1x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl3_y1x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl3_y1x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl3_y1x4.val[ch], x4_col);
                v1_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v1_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl3_y2x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl3_y2x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl3_y2x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl3_y2x4.val[ch], x4_col);
                v2_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v2_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl3_y3x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl3_y3x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl3_y3x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl3_y3x4.val[ch], x4_col);
                v3_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v3_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl3_y4x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl3_y4x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl3_y4x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl3_y4x4.val[ch], x4_col);
                v4_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v4_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                v_lo = sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, q);
                v_hi = sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, q);

                /* Apply Rounding and Shift Right (Q11 -> Integer) */
                v_lo = vrshrq_n_s32(v_lo, SGL_Q11_FRAC_BITS);
                v_hi = vrshrq_n_s32(v_hi, SGL_Q11_FRAC_BITS);

                value3.val[ch] = sgl_simd_clamp_u8_i32(v_lo, v_hi);
            }

            vst3_u8(dst, value3);
            break;
        case SGL_BPP16:
            vtbl2_y1x1 = vld2q_u8(src_y1_buf + x1_off);
            vtbl2_y1x2 = vld2q_u8(src_y1_buf + x2_off);
            vtbl2_y1x3 = vld2q_u8(src_y1_buf + x3_off);
            vtbl2_y1x4 = vld2q_u8(src_y1_buf + x4_off);

            vtbl2_y2x1 = vld2q_u8(src_y2_buf + x1_off);
            vtbl2_y2x2 = vld2q_u8(src_y2_buf + x2_off);
            vtbl2_y2x3 = vld2q_u8(src_y2_buf + x3_off);
            vtbl2_y2x4 = vld2q_u8(src_y2_buf + x4_off);

            vtbl2_y3x1 = vld2q_u8(src_y3_buf + x1_off);
            vtbl2_y3x2 = vld2q_u8(src_y3_buf + x2_off);
            vtbl2_y3x3 = vld2q_u8(src_y3_buf + x3_off);
            vtbl2_y3x4 = vld2q_u8(src_y3_buf + x4_off);

            vtbl2_y4x1 = vld2q_u8(src_y4_buf + x1_off);
            vtbl2_y4x2 = vld2q_u8(src_y4_buf + x2_off);
            vtbl2_y4x3 = vld2q_u8(src_y4_buf + x3_off);
            vtbl2_y4x4 = vld2q_u8(src_y4_buf + x4_off);

            for (ch = 0; ch < SGL_BPP16; ++ch) {
                x1 = vqtbl1_u8(vtbl2_y1x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl2_y1x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl2_y1x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl2_y1x4.val[ch], x4_col);
                v1_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v1_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl2_y2x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl2_y2x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl2_y2x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl2_y2x4.val[ch], x4_col);
                v2_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v2_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl2_y3x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl2_y3x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl2_y3x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl2_y3x4.val[ch], x4_col);
                v3_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v3_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                x1 = vqtbl1_u8(vtbl2_y4x1.val[ch], x1_col);
                x2 = vqtbl1_u8(vtbl2_y4x2.val[ch], x2_col);
                x3 = vqtbl1_u8(vtbl2_y4x3.val[ch], x3_col);
                x4 = vqtbl1_u8(vtbl2_y4x4.val[ch], x4_col);
                v4_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
                v4_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

                v_lo = sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, q);
                v_hi = sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, q);

                /* Apply Rounding and Shift Right (Q11 -> Integer) */
                v_lo = vrshrq_n_s32(v_lo, SGL_Q11_FRAC_BITS);
                v_hi = vrshrq_n_s32(v_hi, SGL_Q11_FRAC_BITS);

                value2.val[ch] = sgl_simd_clamp_u8_i32(v_lo, v_hi);
            }

            vst2_u8(dst, value2);
            break;
        case SGL_BPP8:
            vtbl1_y1x1 = vld1q_u8(src_y1_buf + x1_off);
            vtbl1_y1x2 = vld1q_u8(src_y1_buf + x2_off);
            vtbl1_y1x3 = vld1q_u8(src_y1_buf + x3_off);
            vtbl1_y1x4 = vld1q_u8(src_y1_buf + x4_off);

            vtbl1_y2x1 = vld1q_u8(src_y2_buf + x1_off);
            vtbl1_y2x2 = vld1q_u8(src_y2_buf + x2_off);
            vtbl1_y2x3 = vld1q_u8(src_y2_buf + x3_off);
            vtbl1_y2x4 = vld1q_u8(src_y2_buf + x4_off);

            vtbl1_y3x1 = vld1q_u8(src_y3_buf + x1_off);
            vtbl1_y3x2 = vld1q_u8(src_y3_buf + x2_off);
            vtbl1_y3x3 = vld1q_u8(src_y3_buf + x3_off);
            vtbl1_y3x4 = vld1q_u8(src_y3_buf + x4_off);

            vtbl1_y4x1 = vld1q_u8(src_y4_buf + x1_off);
            vtbl1_y4x2 = vld1q_u8(src_y4_buf + x2_off);
            vtbl1_y4x3 = vld1q_u8(src_y4_buf + x3_off);
            vtbl1_y4x4 = vld1q_u8(src_y4_buf + x4_off);

            x1 = vqtbl1_u8(vtbl1_y1x1, x1_col);
            x2 = vqtbl1_u8(vtbl1_y1x2, x2_col);
            x3 = vqtbl1_u8(vtbl1_y1x3, x3_col);
            x4 = vqtbl1_u8(vtbl1_y1x4, x4_col);
            v1_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v1_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            x1 = vqtbl1_u8(vtbl1_y2x1, x1_col);
            x2 = vqtbl1_u8(vtbl1_y2x2, x2_col);
            x3 = vqtbl1_u8(vtbl1_y2x3, x3_col);
            x4 = vqtbl1_u8(vtbl1_y2x4, x4_col);
            v2_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v2_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            x1 = vqtbl1_u8(vtbl1_y3x1, x1_col);
            x2 = vqtbl1_u8(vtbl1_y3x2, x2_col);
            x3 = vqtbl1_u8(vtbl1_y3x3, x3_col);
            x4 = vqtbl1_u8(vtbl1_y3x4, x4_col);
            v3_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v3_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            x1 = vqtbl1_u8(vtbl1_y4x1, x1_col);
            x2 = vqtbl1_u8(vtbl1_y4x2, x2_col);
            x3 = vqtbl1_u8(vtbl1_y4x3, x3_col);
            x4 = vqtbl1_u8(vtbl1_y4x4, x4_col);
            v4_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v4_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            v_lo = sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, q);
            v_hi = sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, q);

            /* Apply Rounding and Shift Right (Q11 -> Integer) */
            v_lo = vrshrq_n_s32(v_lo, SGL_Q11_FRAC_BITS);
            v_hi = vrshrq_n_s32(v_hi, SGL_Q11_FRAC_BITS);

            value1 = sgl_simd_clamp_u8_i32(v_lo, v_hi);

            vst1_u8(dst, value1);
            break;
        }

        dst += step;
    }

    return dst;
}

static SGL_ALWAYS_INLINE uint8_t *sgl_simd_resize_bicubic_downscale_line_stripe(
                                    int32_t row, int32_t num_lanes, int32_t step, int32_t bpp,
                                    sgl_bicubic_data_t *data)
{
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    int32_t col, ch, lane;
    uint8_t *src_y1_buf, *src_y2_buf, *src_y3_buf, *src_y4_buf;
    uint8_t *dst;

    sgl_simd_q11_t p;
    sgl_simd_q11_ext_t p_lo, p_hi, q;

    uint8x8_t x1, x2, x3, x4;

    sgl_simd_q11_ext_t v1_lo, v2_lo, v3_lo, v4_lo;
    sgl_simd_q11_ext_t v1_hi, v2_hi, v3_hi, v4_hi;
    sgl_simd_q11_ext_t v_lo, v_hi;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1; 
    
    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
    
    /* set 'row' data */
    q = vdupq_n_s32((int32_t)row_lookup->q[row]);

    src_y1_buf = data->src + (row_lookup->y1[row] * data->src_stride);
    src_y2_buf = data->src + (row_lookup->y2[row] * data->src_stride);
    src_y3_buf = data->src + (row_lookup->y3[row] * data->src_stride);
    src_y4_buf = data->src + (row_lookup->y4[row] * data->src_stride);
    dst = data->dst + (row * data->dst_stride);

    for (lane = 0; lane < num_lanes; ++lane) {
        col = (lane * NEON_LANE_SIZE);
        p = vld1q_s16(&col_lookup->p[col]);
        p_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_s16(p)));
        p_hi = vreinterpretq_s32_u32(vmovl_high_u16(p));

        for (ch = 0; ch < bpp; ++ch) {
            x1 = sgl_neon_vset_u8(src_y1_buf, &col_lookup->x1[col], ch, bpp);
            x2 = sgl_neon_vset_u8(src_y1_buf, &col_lookup->x2[col], ch, bpp);
            x3 = sgl_neon_vset_u8(src_y1_buf, &col_lookup->x3[col], ch, bpp);
            x4 = sgl_neon_vset_u8(src_y1_buf, &col_lookup->x4[col], ch, bpp);
            v1_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v1_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            x1 = sgl_neon_vset_u8(src_y2_buf, &col_lookup->x1[col], ch, bpp);
            x2 = sgl_neon_vset_u8(src_y2_buf, &col_lookup->x2[col], ch, bpp);
            x3 = sgl_neon_vset_u8(src_y2_buf, &col_lookup->x3[col], ch, bpp);
            x4 = sgl_neon_vset_u8(src_y2_buf, &col_lookup->x4[col], ch, bpp);
            v2_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v2_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            x1 = sgl_neon_vset_u8(src_y3_buf, &col_lookup->x1[col], ch, bpp);
            x2 = sgl_neon_vset_u8(src_y3_buf, &col_lookup->x2[col], ch, bpp);
            x3 = sgl_neon_vset_u8(src_y3_buf, &col_lookup->x3[col], ch, bpp);
            x4 = sgl_neon_vset_u8(src_y3_buf, &col_lookup->x4[col], ch, bpp);
            v3_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v3_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            x1 = sgl_neon_vset_u8(src_y4_buf, &col_lookup->x1[col], ch, bpp);
            x2 = sgl_neon_vset_u8(src_y4_buf, &col_lookup->x2[col], ch, bpp);
            x3 = sgl_neon_vset_u8(src_y4_buf, &col_lookup->x3[col], ch, bpp);
            x4 = sgl_neon_vset_u8(src_y4_buf, &col_lookup->x4[col], ch, bpp);
            v4_lo = sgl_neon_bicubic_interpolation_lo(x1, x2, x3, x4, p_lo);
            v4_hi = sgl_neon_bicubic_interpolation_hi(x1, x2, x3, x4, p_hi);

            v_lo = sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, q);
            v_hi = sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, q);

            /* Apply Rounding and Shift Right (Q11 -> Integer) */
            v_lo = vrshrq_n_s32(v_lo, SGL_Q11_FRAC_BITS);
            v_hi = vrshrq_n_s32(v_hi, SGL_Q11_FRAC_BITS);

            switch (bpp) {
            case SGL_BPP32:
                value4.val[ch] = sgl_simd_clamp_u8_i32(v_lo, v_hi);
                break;
            case SGL_BPP24:
                value3.val[ch] = sgl_simd_clamp_u8_i32(v_lo, v_hi);
                break;
            case SGL_BPP16:
                value2.val[ch] = sgl_simd_clamp_u8_i32(v_lo, v_hi);
                break;
            case SGL_BPP8:
                value1 = sgl_simd_clamp_u8_i32(v_lo, v_hi);
                break;
            }
        }

        switch (bpp) {
        case SGL_BPP32:
            vst4_u8(dst, value4);
            break;
        case SGL_BPP24:
            vst3_u8(dst, value3);
            break;
        case SGL_BPP16:
            vst2_u8(dst, value2);
            break;
        case SGL_BPP8:
            vst1_u8(dst, value1);
            break;
        }

        dst += step;
    }

    return dst;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_bicubic_line_stripe(int32_t row, sgl_bicubic_data_t *data) {
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    int32_t col; 
    int32_t d_width, bpp, step;
    int32_t x1_off, x2_off, x3_off, x4_off;
    int32_t y1, y2, y3, y4;
    sgl_q11_t p, q;
    sgl_q11_ext_t v1, v2, v3, v4, value;
    uint8_t *src, *dst;
    int32_t ch, src_stride;
    uint8_t *src_y1_buf, *src_y2_buf, *src_y3_buf, *src_y4_buf;
    uint8_t *src_y1x1, *src_y1x2, *src_y1x3, *src_y1x4;
    uint8_t *src_y2x1, *src_y2x2, *src_y2x3, *src_y2x4;
    uint8_t *src_y3x1, *src_y3x2, *src_y3x3, *src_y3x4;
    uint8_t *src_y4x1, *src_y4x2, *src_y4x3, *src_y4x4;

    int32_t num_lanes;

    d_width = data->lut->d_width;
    bpp = data->bpp;
    num_lanes = d_width / NEON_LANE_SIZE;
    step = bpp * NEON_LANE_SIZE;
    
    if (data->src_stride <= data->dst_stride) {
        dst = sgl_simd_resize_bicubic_upscale_line_stripe(row, num_lanes, step, bpp, data);
    }
    else {
        dst = sgl_simd_resize_bicubic_downscale_line_stripe(row, num_lanes, step, bpp, data);
    }

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;

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

    for (col = num_lanes * NEON_LANE_SIZE; col < d_width; ++col) {
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

sgl_result_t sgl_simd_resize_bicubic(
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
                    sgl_simd_resize_bicubic_line_stripe(row, (void *)&data);
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
                    sgl_threadpool_attach_routine(pool, sgl_simd_resize_bicubic_routine, operations, (void *)&data);
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
static void sgl_simd_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    sgl_bicubic_current_t *cur = (sgl_bicubic_current_t *)current;
    sgl_bicubic_data_t *data = (sgl_bicubic_data_t *)cookie;
    int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_simd_resize_bicubic_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
