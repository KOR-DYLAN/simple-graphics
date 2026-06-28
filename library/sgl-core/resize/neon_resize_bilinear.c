/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
/* SGL-CALLBACK-DEV-001: thread callbacks recover typed context from void *. */
/* cppcheck-suppress-file misra-c2012-11.5 */
/* cppcheck-suppress-file constParameterCallback */
#include <arm_neon.h>
#include <sgl-core.h>
#include "bilinear.h"

#define NEON_LANE_SIZE  (8)
#define SGL_SIMD_BILINEAR_CACHE_BULK_SIZE (32)
#define SGL_SIMD_BILINEAR_SEPARABLE_BITS  (SGL_Q11_FRAC_BITS * 2)
#define SGL_SIMD_BILINEAR_SEPARABLE_HALF  (1 << (SGL_SIMD_BILINEAR_SEPARABLE_BITS - 1))

typedef struct {
    sgl_int32_t y;
    sgl_q11_ext_t *SGL_RESTRICT row;
} sgl_simd_bilinear_row_cache_t;

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

static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_bilinear_make_pixel_offsets(
    const sgl_int32_t *SGL_RESTRICT x)
{
    int32x4_t base;
    int32x4_t offset_low;
    int32x4_t offset_high;
    uint16x4_t offset_u16_low;
    uint16x4_t offset_u16_high;
    uint8x8_t result;

    /*
     * Keep the shared LUT bpp-independent.  The table lookup index is a SIMD
     * local pixel delta from the first source column in this 8-lane group.
     */
    base = vdupq_n_s32(x[0]);
    offset_low = vsubq_s32(vld1q_s32(&x[0]), base);
    offset_high = vsubq_s32(vld1q_s32(&x[4]), base);
    offset_u16_low = vqmovun_s32(offset_low);
    offset_u16_high = vqmovun_s32(offset_high);
    result = vqmovn_u16(vcombine_u16(offset_u16_low, offset_u16_high));

    return result;
}

/*
 * Design and Operation
 * --------------------
 * The direct SIMD path gathers four source pixels for every destination row.
 * Adjacent bilinear rows often share one source row, so the bpp32 fast path
 * below separates the work:
 *
 *   source row y ---- horizontal Q11 row cache ----+
 *                                                   +-- NEON vertical mix
 *   source row y+1 -- horizontal Q11 row cache ----+
 *
 * This trades a small two-row Q11 scratch buffer for fewer repeated source
 * loads and fewer gather/table operations.  The cache lives inside a row range,
 * so each worker thread can reuse nearby rows without synchronization.
 */
static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_bilinear_pack_u8(
    int32x4_t lo,
    int32x4_t hi)
{
    uint16x4_t u16_lo;
    uint16x4_t u16_hi;
    uint8x8_t result;

    u16_lo = vreinterpret_u16_s16(vmovn_s32(lo));
    u16_hi = vreinterpret_u16_s16(vmovn_s32(hi));
    result = vmovn_u16(vcombine_u16(u16_lo, u16_hi));

    return result;
}

static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_bilinear_vertical_q11_to_u8(
    int32x4_t top_lo,
    int32x4_t bottom_lo,
    int32x4_t top_hi,
    int32x4_t bottom_hi,
    int32x4_t vec_q)
{
    int32x4_t acc_lo;
    int32x4_t acc_hi;
    int32x4_t half;

    half = vdupq_n_s32(SGL_SIMD_BILINEAR_SEPARABLE_HALF);
    acc_lo = vmulq_n_s32(top_lo, SGL_Q11_ONE);
    acc_hi = vmulq_n_s32(top_hi, SGL_Q11_ONE);
    acc_lo = vmlaq_s32(acc_lo, vsubq_s32(bottom_lo, top_lo), vec_q);
    acc_hi = vmlaq_s32(acc_hi, vsubq_s32(bottom_hi, top_hi), vec_q);
    acc_lo = vshrq_n_s32(vaddq_s32(acc_lo, half),
                         SGL_SIMD_BILINEAR_SEPARABLE_BITS);
    acc_hi = vshrq_n_s32(vaddq_s32(acc_hi, half),
                         SGL_SIMD_BILINEAR_SEPARABLE_BITS);

    return sgl_neon_bilinear_pack_u8(acc_lo, acc_hi);
}

static SGL_ALWAYS_INLINE sgl_uint8_t sgl_simd_bilinear_separable_acc_to_u8(
    sgl_q11_ext_t acc)
{
    sgl_q11_ext_t value;

    value = (acc + SGL_SIMD_BILINEAR_SEPARABLE_HALF) >>
            SGL_SIMD_BILINEAR_SEPARABLE_BITS;

    return (sgl_uint8_t)value;
}

static SGL_ALWAYS_INLINE void sgl_simd_bilinear_horizontal_bpp32(
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
        dst_off = col * SGL_BPP32;
        x1_off = col_lookup->x1[col] * SGL_BPP32;
        x2_off = col_lookup->x2[col] * SGL_BPP32;
        p = (sgl_q11_ext_t)col_lookup->p[col];

        src0 = (sgl_q11_ext_t)src_row[x1_off];
        src1 = (sgl_q11_ext_t)src_row[x2_off];
        dst_row[dst_off] =
            (src0 * SGL_Q11_ONE) + ((src1 - src0) * p);

        src0 = (sgl_q11_ext_t)src_row[x1_off + 1];
        src1 = (sgl_q11_ext_t)src_row[x2_off + 1];
        dst_row[dst_off + 1] =
            (src0 * SGL_Q11_ONE) + ((src1 - src0) * p);

        src0 = (sgl_q11_ext_t)src_row[x1_off + 2];
        src1 = (sgl_q11_ext_t)src_row[x2_off + 2];
        dst_row[dst_off + 2] =
            (src0 * SGL_Q11_ONE) + ((src1 - src0) * p);

        src0 = (sgl_q11_ext_t)src_row[x1_off + 3];
        src1 = (sgl_q11_ext_t)src_row[x2_off + 3];
        dst_row[dst_off + 3] =
            (src0 * SGL_Q11_ONE) + ((src1 - src0) * p);
    }
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t *sgl_simd_bilinear_get_cached_row_bpp32(
    sgl_simd_bilinear_row_cache_t *SGL_RESTRICT cache,
    sgl_int32_t y,
    const sgl_bilinear_data_t *SGL_RESTRICT data)
{
    sgl_simd_bilinear_row_cache_t *slot;
    const sgl_uint8_t *src_row;
    sgl_int32_t d_width;

    d_width = data->lut->d_width;
    slot = &cache[y & 1];
    if (slot->y != y) {
        src_row = &data->src[y * data->src_stride];
        sgl_simd_bilinear_horizontal_bpp32(
            src_row,
            slot->row,
            &data->lut->col_lookup,
            d_width);
        slot->y = y;
    }

    return slot->row;
}

static SGL_ALWAYS_INLINE void sgl_simd_bilinear_vertical_bpp32(
    sgl_uint8_t *SGL_RESTRICT dst_row,
    const sgl_q11_ext_t *SGL_RESTRICT top_row,
    const sgl_q11_ext_t *SGL_RESTRICT bottom_row,
    sgl_q11_ext_t q,
    sgl_int32_t row_width)
{
    sgl_int32_t off;
    int32x4_t vec_q;
    int32x4_t top_lo;
    int32x4_t top_hi;
    int32x4_t bottom_lo;
    int32x4_t bottom_hi;
    sgl_q11_ext_t top;
    sgl_q11_ext_t bottom;

    vec_q = vdupq_n_s32(q);
    off = 0;
    while ((off + NEON_LANE_SIZE) <= row_width) {
        top_lo = vld1q_s32(&top_row[off]);
        bottom_lo = vld1q_s32(&bottom_row[off]);
        top_hi = vld1q_s32(&top_row[off + 4]);
        bottom_hi = vld1q_s32(&bottom_row[off + 4]);
        vst1_u8(&dst_row[off],
                sgl_neon_bilinear_vertical_q11_to_u8(
                    top_lo, bottom_lo, top_hi, bottom_hi, vec_q));
        off += NEON_LANE_SIZE;
    }

    while (off < row_width) {
        top = top_row[off];
        bottom = bottom_row[off];
        dst_row[off] = sgl_simd_bilinear_separable_acc_to_u8(
            (top * SGL_Q11_ONE) + ((bottom - top) * q));
        ++off;
    }
}

static sgl_result_t sgl_simd_resize_bilinear_range_separable_bpp32(
    sgl_bilinear_data_t *data,
    sgl_int32_t start_row,
    sgl_int32_t row_count)
{
    sgl_result_t result;
    sgl_simd_bilinear_row_cache_t cache[2];
    sgl_q11_ext_t *row_storage;
    sgl_q11_ext_t *top_row;
    sgl_q11_ext_t *bottom_row;
    sgl_int32_t row;
    sgl_int32_t end_row;
    sgl_int32_t d_height;
    sgl_int32_t row_width;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_uint8_t *dst_row;

    result = SGL_SUCCESS;
    row_storage = SGL_NULL;
    d_height = data->lut->d_height;
    row_width = data->lut->d_width * SGL_BPP32;
    end_row = start_row + row_count;
    if (end_row > d_height) {
        end_row = d_height;
    }

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    row_storage = (sgl_q11_ext_t *)sgl_malloc(
        sizeof(sgl_q11_ext_t) * (sgl_size_t)row_width * 2U);

    if (row_storage != SGL_NULL) {
        cache[0].y = -1;
        cache[0].row = row_storage;
        cache[1].y = -1;
        cache[1].row = &row_storage[row_width];

        for (row = start_row; row < end_row; ++row) {
            y1 = data->lut->row_lookup.y1[row];
            y2 = data->lut->row_lookup.y2[row];
            top_row = sgl_simd_bilinear_get_cached_row_bpp32(cache, y1, data);
            bottom_row = sgl_simd_bilinear_get_cached_row_bpp32(cache, y2, data);
            dst_row = &data->dst[row * data->dst_stride];
            sgl_simd_bilinear_vertical_bpp32(
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

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bilinear_upscale_line_stripe_bpp32(
                                    sgl_int32_t row, sgl_int32_t num_lanes,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t lane;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    sgl_uint8_t *dst;

    sgl_simd_q11_t vec_p;
    sgl_simd_q11_t vec_p_inv;
    sgl_simd_q11_t vec_q;
    sgl_simd_q11_t vec_q_inv;

    sgl_simd_q11_t vec_w00;
    sgl_simd_q11_t vec_w01;
    sgl_simd_q11_t vec_w10;
    sgl_simd_q11_t vec_w11;

    uint8x8x4_t value;
    uint8x16x4_t vtbl_src_y1x1;
    uint8x16x4_t vtbl_src_y1x2;
    uint8x16x4_t vtbl_src_y2x1;
    uint8x16x4_t vtbl_src_y2x2;
    uint8x8_t vec_src_y1x1;
    uint8x8_t vec_src_y1x2;
    uint8x8_t vec_src_y2x1;
    uint8x8_t vec_src_y2x2;
    uint8x8_t vec_x1_col;
    uint8x8_t vec_x2_col;

    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;

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

        x1_off = col_lookup->x1[col] * SGL_BPP32;
        x2_off = col_lookup->x2[col] * SGL_BPP32;

        vec_x1_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x1[col]);
        vec_x2_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x2[col]);

        vtbl_src_y1x1 = vld4q_u8(&src_y1_buf[x1_off]);
        vtbl_src_y1x2 = vld4q_u8(&src_y1_buf[x2_off]);
        vtbl_src_y2x1 = vld4q_u8(&src_y2_buf[x1_off]);
        vtbl_src_y2x2 = vld4q_u8(&src_y2_buf[x2_off]);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[0], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[0], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[0], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[0], vec_x2_col);
        value.val[0] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[1], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[1], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[1], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[1], vec_x2_col);
        value.val[1] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[2], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[2], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[2], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[2], vec_x2_col);
        value.val[2] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[3], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[3], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[3], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[3], vec_x2_col);
        value.val[3] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vst4_u8(dst, value);
        dst = &dst[SGL_BPP32 * NEON_LANE_SIZE];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bilinear_upscale_line_stripe_bpp24(
                                    sgl_int32_t row, sgl_int32_t num_lanes,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t lane;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    sgl_uint8_t *dst;
    sgl_simd_q11_t vec_p;
    sgl_simd_q11_t vec_p_inv;
    sgl_simd_q11_t vec_q;
    sgl_simd_q11_t vec_q_inv;
    sgl_simd_q11_t vec_w00;
    sgl_simd_q11_t vec_w01;
    sgl_simd_q11_t vec_w10;
    sgl_simd_q11_t vec_w11;
    uint8x8x3_t value;
    uint8x16x3_t vtbl_src_y1x1;
    uint8x16x3_t vtbl_src_y1x2;
    uint8x16x3_t vtbl_src_y2x1;
    uint8x16x3_t vtbl_src_y2x2;
    uint8x8_t vec_src_y1x1;
    uint8x8_t vec_src_y1x2;
    uint8x8_t vec_src_y2x1;
    uint8x8_t vec_src_y2x2;
    uint8x8_t vec_x1_col;
    uint8x8_t vec_x2_col;

    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
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
        x1_off = col_lookup->x1[col] * SGL_BPP24;
        x2_off = col_lookup->x2[col] * SGL_BPP24;
        vec_x1_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x1[col]);
        vec_x2_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x2[col]);

        vtbl_src_y1x1 = vld3q_u8(&src_y1_buf[x1_off]);
        vtbl_src_y1x2 = vld3q_u8(&src_y1_buf[x2_off]);
        vtbl_src_y2x1 = vld3q_u8(&src_y2_buf[x1_off]);
        vtbl_src_y2x2 = vld3q_u8(&src_y2_buf[x2_off]);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[0], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[0], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[0], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[0], vec_x2_col);
        value.val[0] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[1], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[1], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[1], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[1], vec_x2_col);
        value.val[1] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[2], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[2], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[2], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[2], vec_x2_col);
        value.val[2] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vst3_u8(dst, value);
        dst = &dst[SGL_BPP24 * NEON_LANE_SIZE];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bilinear_upscale_line_stripe_bpp16(
                                    sgl_int32_t row, sgl_int32_t num_lanes,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t lane;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    sgl_uint8_t *dst;
    sgl_simd_q11_t vec_p;
    sgl_simd_q11_t vec_p_inv;
    sgl_simd_q11_t vec_q;
    sgl_simd_q11_t vec_q_inv;
    sgl_simd_q11_t vec_w00;
    sgl_simd_q11_t vec_w01;
    sgl_simd_q11_t vec_w10;
    sgl_simd_q11_t vec_w11;
    uint8x8x2_t value;
    uint8x16x2_t vtbl_src_y1x1;
    uint8x16x2_t vtbl_src_y1x2;
    uint8x16x2_t vtbl_src_y2x1;
    uint8x16x2_t vtbl_src_y2x2;
    uint8x8_t vec_src_y1x1;
    uint8x8_t vec_src_y1x2;
    uint8x8_t vec_src_y2x1;
    uint8x8_t vec_src_y2x2;
    uint8x8_t vec_x1_col;
    uint8x8_t vec_x2_col;

    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
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
        x1_off = col_lookup->x1[col] * SGL_BPP16;
        x2_off = col_lookup->x2[col] * SGL_BPP16;
        vec_x1_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x1[col]);
        vec_x2_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x2[col]);

        vtbl_src_y1x1 = vld2q_u8(&src_y1_buf[x1_off]);
        vtbl_src_y1x2 = vld2q_u8(&src_y1_buf[x2_off]);
        vtbl_src_y2x1 = vld2q_u8(&src_y2_buf[x1_off]);
        vtbl_src_y2x2 = vld2q_u8(&src_y2_buf[x2_off]);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[0], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[0], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[0], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[0], vec_x2_col);
        value.val[0] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1.val[1], vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2.val[1], vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1.val[1], vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2.val[1], vec_x2_col);
        value.val[1] = sgl_neon_bilinear_interpolation(
                            vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                            vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                            vec_w00, vec_w01, vec_w10, vec_w11);

        vst2_u8(dst, value);
        dst = &dst[SGL_BPP16 * NEON_LANE_SIZE];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bilinear_upscale_line_stripe_bpp8(
                                    sgl_int32_t row, sgl_int32_t num_lanes,
                                    sgl_bilinear_data_t *data)
{
    bilinear_column_lookup_t *col_lookup;
    bilinear_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t lane;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    sgl_uint8_t *dst;
    sgl_simd_q11_t vec_p;
    sgl_simd_q11_t vec_p_inv;
    sgl_simd_q11_t vec_q;
    sgl_simd_q11_t vec_q_inv;
    sgl_simd_q11_t vec_w00;
    sgl_simd_q11_t vec_w01;
    sgl_simd_q11_t vec_w10;
    sgl_simd_q11_t vec_w11;
    uint8x16_t vtbl_src_y1x1;
    uint8x16_t vtbl_src_y1x2;
    uint8x16_t vtbl_src_y2x1;
    uint8x16_t vtbl_src_y2x2;
    uint8x8_t vec_src_y1x1;
    uint8x8_t vec_src_y1x2;
    uint8x8_t vec_src_y2x1;
    uint8x8_t vec_src_y2x2;
    uint8x8_t vec_x1_col;
    uint8x8_t vec_x2_col;
    uint8x8_t value;

    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;
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
        x1_off = col_lookup->x1[col];
        x2_off = col_lookup->x2[col];
        vec_x1_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x1[col]);
        vec_x2_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x2[col]);

        vtbl_src_y1x1 = vld1q_u8(&src_y1_buf[x1_off]);
        vtbl_src_y1x2 = vld1q_u8(&src_y1_buf[x2_off]);
        vtbl_src_y2x1 = vld1q_u8(&src_y2_buf[x1_off]);
        vtbl_src_y2x2 = vld1q_u8(&src_y2_buf[x2_off]);
        vec_src_y1x1 = vqtbl1_u8(vtbl_src_y1x1, vec_x1_col);
        vec_src_y1x2 = vqtbl1_u8(vtbl_src_y1x2, vec_x2_col);
        vec_src_y2x1 = vqtbl1_u8(vtbl_src_y2x1, vec_x1_col);
        vec_src_y2x2 = vqtbl1_u8(vtbl_src_y2x2, vec_x2_col);
        value = sgl_neon_bilinear_interpolation(
                    vmovl_u8(vec_src_y1x1), vmovl_u8(vec_src_y1x2),
                    vmovl_u8(vec_src_y2x1), vmovl_u8(vec_src_y2x2),
                    vec_w00, vec_w01, vec_w10, vec_w11);
        vst1_u8(dst, value);
        dst = &dst[SGL_BPP8 * NEON_LANE_SIZE];
    }

    return dst;
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
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_uint8_t *src_y1_buf;
    sgl_uint8_t *src_y2_buf;
    sgl_uint8_t *dst;

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
        vec_p = vld1q_s16(&col_lookup->p[col]);
        vec_p_inv = vld1q_s16(&col_lookup->inv_p[col]);

        vec_w00 = sgl_simd_q11_mul(vec_p_inv, vec_q_inv);
        vec_w01 = sgl_simd_q11_mul(vec_p, vec_q_inv);
        vec_w10 = sgl_simd_q11_mul(vec_p_inv, vec_q);
        vec_w11 = sgl_simd_q11_mul(vec_p, vec_q);

        col = (lane * NEON_LANE_SIZE);
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;

        vec_x1_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x1[col]);
        vec_x2_col = sgl_neon_bilinear_make_pixel_offsets(&col_lookup->x2[col]);

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

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;

    if (data->src_stride <= data->dst_stride) {
        switch (bpp) {
        case SGL_BPP32:
            dst = sgl_simd_resize_bilinear_upscale_line_stripe_bpp32(row, num_lanes, data);
            break;
        case SGL_BPP24:
            dst = sgl_simd_resize_bilinear_upscale_line_stripe_bpp24(row, num_lanes, data);
            break;
        case SGL_BPP16:
            dst = sgl_simd_resize_bilinear_upscale_line_stripe_bpp16(row, num_lanes, data);
            break;
        case SGL_BPP8:
            dst = sgl_simd_resize_bilinear_upscale_line_stripe_bpp8(row, num_lanes, data);
            break;
        default:
            dst = sgl_simd_resize_bilinear_upscale_line_stripe(row, num_lanes, step, bpp, data);
            break;
        }
    }
    else {
        dst = sgl_simd_resize_bilinear_downscale_line_stripe(row, num_lanes, step, bpp, data);
    }

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
        if ((d_width == s_width) && (d_height == s_height)) {
            (void)sgl_memcpy(dst, src, (sgl_size_t)d_width * (sgl_size_t)d_height * (sgl_size_t)bpp);
        }
        else if (ext_lut != SGL_NULL) {
            if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
                (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
            {
                /* apply external look-up table */
                lut = ext_lut;
            }
        }

        if ((d_width != s_width) || (d_height != s_height)) {
            if (lut == SGL_NULL) {
                /* create temp look-up table */
                temp_lut = sgl_generic_create_bilinear_lut(d_width, d_height, s_width, s_height);
                lut = temp_lut;
            }
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
                switch (bpp) {
                case SGL_BPP32:
                    result = sgl_simd_resize_bilinear_range_separable_bpp32(
                        &data, 0, d_height);
                    if (result != SGL_SUCCESS) {
                        result = SGL_SUCCESS;
                        for (row = 0; row < d_height; ++row) {
                            sgl_simd_resize_bilinear_line_stripe(row, (void *)&data);
                        }
                    }
                    else {
                        /* bpp32 separable row-cache path completed */
                    }
                    break;
                default:
                    for (row = 0; row < d_height; ++row) {
                        sgl_simd_resize_bilinear_line_stripe(row, (void *)&data);
                    }
                    break;
                }
            }
#if defined(SGL_CFG_HAS_THREAD)
            else {
                sgl_bilinear_current_t *currents;
                sgl_queue_t *operations = SGL_NULL;
                sgl_int32_t i;
                sgl_int32_t num_operations;
                sgl_int32_t mod_operations;
                sgl_int32_t bulk_size;

                switch (bpp) {
                case SGL_BPP32:
                    bulk_size = SGL_SIMD_BILINEAR_CACHE_BULK_SIZE;
                    break;
                default:
                    bulk_size = SGL_SIMD_BULK_SIZE;
                    break;
                }

                num_operations = d_height / bulk_size;
                mod_operations = d_height % bulk_size;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((sgl_size_t)num_operations);
                /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
                /* cppcheck-suppress misra-c2012-11.5 */
                currents = (sgl_bilinear_current_t *)sgl_malloc(sizeof(sgl_bilinear_current_t) * (sgl_size_t)num_operations);
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
    sgl_result_t result;
    sgl_int32_t row;

    result = SGL_ERROR_NOT_SUPPORTED;
    switch (data->bpp) {
    case SGL_BPP32:
        result = sgl_simd_resize_bilinear_range_separable_bpp32(
            data, cur->row, cur->count);
        break;
    default:
        break;
    }

    if (result != SGL_SUCCESS) {
        for (row = cur->row; row < (cur->row + cur->count); ++row) {
            sgl_simd_resize_bilinear_line_stripe(row, data);
        }
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
