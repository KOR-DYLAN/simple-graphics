/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "bicubic.h"
#include "resize_bitops.h"
#include "resize_prefetch.h"
#include "sgl_trace.h"
#include "threaded_resize.h"

#define NEON_LANE_SHIFT (3U)
#define NEON_LANE_SIZE  ((sgl_int32_t)(1U << NEON_LANE_SHIFT))
#define NEON_LANE_OFFSET(lane) \
    ((sgl_int32_t)((sgl_uint32_t)(lane) << NEON_LANE_SHIFT))
#define NEON_LANE_COUNT(width) \
    ((sgl_int32_t)((sgl_uint32_t)(width) >> NEON_LANE_SHIFT))
#define NEON_LANE_BYTE_STEP(bpp) \
    ((sgl_int32_t)((sgl_uint32_t)(bpp) << NEON_LANE_SHIFT))
#define SGL_SIMD_BICUBIC_CACHE_BULK_SIZE (32)
#define SGL_SIMD_BICUBIC_TAP_COUNT (4)
#define SGL_SIMD_BICUBIC_ROW_CACHE_COUNT SGL_SIMD_BICUBIC_TAP_COUNT
#define SGL_SIMD_BICUBIC_ROW_CACHE_MASK \
    (SGL_SIMD_BICUBIC_ROW_CACHE_COUNT - 1)

typedef struct {
    sgl_int32_t y;
    sgl_q11_ext_t *SGL_RESTRICT row;
} sgl_simd_bicubic_row_cache_t;

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_vset_u8(const sgl_uint8_t *y_buf, const sgl_int32_t *x, sgl_int32_t ch, sgl_int32_t bpp)
{
    uint8x8_t vec = vdup_n_u8(0);

    vec = vset_lane_u8(y_buf[(x[0] * bpp) + ch], vec, 0);
    vec = vset_lane_u8(y_buf[(x[1] * bpp) + ch], vec, 1);
    vec = vset_lane_u8(y_buf[(x[2] * bpp) + ch], vec, 2);
    vec = vset_lane_u8(y_buf[(x[3] * bpp) + ch], vec, 3);
    vec = vset_lane_u8(y_buf[(x[4] * bpp) + ch], vec, 4);
    vec = vset_lane_u8(y_buf[(x[5] * bpp) + ch], vec, 5);
    vec = vset_lane_u8(y_buf[(x[6] * bpp) + ch], vec, 6);
    vec = vset_lane_u8(y_buf[(x[7] * bpp) + ch], vec, 7);

    return vec;
}

static SGL_ALWAYS_INLINE uint8x8_t sgl_neon_vld_col(const sgl_int32_t *lut, sgl_int32_t col_base)
{
    uint8x8_t vec = vdup_n_u8(0);

    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[0] - col_base), vec, 0);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[1] - col_base), vec, 1);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[2] - col_base), vec, 2);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[3] - col_base), vec, 3);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[4] - col_base), vec, 4);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[5] - col_base), vec, 5);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[6] - col_base), vec, 6);
    vec = vset_lane_u8(sgl_clamp_u8_i32(lut[7] - col_base), vec, 7);

    return vec;

}

static SGL_ALWAYS_INLINE sgl_q11_ext_t sgl_generic_bicubic_interpolation(sgl_q11_ext_t v1, sgl_q11_ext_t v2, sgl_q11_ext_t v3, sgl_q11_ext_t v4, sgl_q11_ext_t d)
{
    sgl_q11_ext_t v;
    sgl_q11_ext_t p1;
    sgl_q11_ext_t p2;
    sgl_q11_ext_t p3;
    sgl_q11_ext_t p4;

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
    sgl_simd_q11_ext_t v;
    sgl_simd_q11_ext_t p1;
    sgl_simd_q11_ext_t p2;
    sgl_simd_q11_ext_t p3;
    sgl_simd_q11_ext_t p4;

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

    v = sgl_simd_q11_ext_mul(d, p4);
    v = vaddq_s32(p3, v);

    v = sgl_simd_q11_ext_mul(d, v);
    v = vaddq_s32(p2, v);

    v = sgl_simd_q11_ext_mul(d, v);
    v = vaddq_s32(p1, v);

    return vshrq_n_s32(v, 1);
}

static SGL_ALWAYS_INLINE sgl_simd_q11_ext_t sgl_neon_bicubic_interpolation_lo(uint8x8_t v1, uint8x8_t v2, uint8x8_t v3, uint8x8_t v4, sgl_simd_q11_ext_t d)
{
    sgl_simd_q11_ext_t v1_lo;
    sgl_simd_q11_ext_t v2_lo;
    sgl_simd_q11_ext_t v3_lo;
    sgl_simd_q11_ext_t v4_lo;

    v1_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v1))), SGL_Q11_FRAC_BITS));
    v2_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v2))), SGL_Q11_FRAC_BITS));
    v3_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v3))), SGL_Q11_FRAC_BITS));
    v4_lo = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(v4))), SGL_Q11_FRAC_BITS));

    return sgl_neon_bicubic_interpolation(v1_lo, v2_lo, v3_lo, v4_lo, d);
}

static SGL_ALWAYS_INLINE sgl_simd_q11_ext_t sgl_neon_bicubic_interpolation_hi(uint8x8_t v1, uint8x8_t v2, uint8x8_t v3, uint8x8_t v4, sgl_simd_q11_ext_t d)
{
    sgl_simd_q11_ext_t v1_hi;
    sgl_simd_q11_ext_t v2_hi;
    sgl_simd_q11_ext_t v3_hi;
    sgl_simd_q11_ext_t v4_hi;

    v1_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v1)), SGL_Q11_FRAC_BITS));
    v2_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v2)), SGL_Q11_FRAC_BITS));
    v3_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v3)), SGL_Q11_FRAC_BITS));
    v4_hi = vreinterpretq_s32_u32(vshlq_n_u32(vmovl_high_u16(vmovl_u8(v4)), SGL_Q11_FRAC_BITS));

    return sgl_neon_bicubic_interpolation(v1_hi, v2_hi, v3_hi, v4_hi, d);

}

static SGL_ALWAYS_INLINE int32x4_t sgl_neon_bicubic_load_pixel_q11_bpp32(
    const sgl_uint8_t *SGL_RESTRICT src)
{
    uint32x2_t packed;
    uint16x8_t u16;
    uint32x4_t u32;

    /* cppcheck-suppress misra-c2012-11.3 */
    packed = vld1_dup_u32((const sgl_uint32_t *)src);
    u16 = vmovl_u8(vreinterpret_u8_u32(packed));
    u32 = vmovl_u16(vget_low_u16(u16));

    return vreinterpretq_s32_u32(vshlq_n_u32(u32, SGL_Q11_FRAC_BITS));
}

static SGL_ALWAYS_INLINE void sgl_neon_bicubic_store_pixel_bpp32(
    sgl_uint8_t *SGL_RESTRICT dst,
    int32x4_t value)
{
    uint8x8_t u8;
    uint32x2_t packed;

    u8 = sgl_simd_clamp_u8_i32(value, vdupq_n_s32(0));
    packed = vreinterpret_u32_u8(u8);
    /* cppcheck-suppress misra-c2012-11.3 */
    vst1_lane_u32((sgl_uint32_t *)dst, packed, 0);
}

/*
 * Downscale bpp32 path
 * --------------------
 * The general downscale kernel builds an eight-lane vector with scalar byte
 * inserts for every source tap and channel.  For RGBA, one complete pixel is
 * already the natural four-lane vector:
 *
 *   4 source rows x 4 source columns x [R G B A]
 *                  |                         |
 *                  +---- horizontal cubic --+
 *                              |
 *                        vertical cubic
 *                              |
 *                         [R G B A] dst
 *
 * Sixteen 32-bit pixel loads replace sixty-four scalar byte inserts per output
 * pixel.  The Q11 interpolation order is unchanged, so this is bit-identical
 * to the existing kernel while leaving every upscale branch untouched.
 */
static SGL_ALWAYS_INLINE void sgl_simd_resize_bicubic_downscale_line_bpp32(
    sgl_int32_t row,
    sgl_bicubic_data_t *SGL_RESTRICT data)
{
    const bicubic_column_lookup_t *col_lookup;
    const bicubic_row_lookup_t *row_lookup;
    const sgl_uint8_t *src_rows[SGL_SIMD_BICUBIC_TAP_COUNT];
    sgl_uint8_t *dst;
    sgl_int32_t offsets[SGL_SIMD_BICUBIC_TAP_COUNT];
    sgl_int32_t col;
    sgl_int32_t src_row;
    int32x4_t p;
    int32x4_t q;
    int32x4_t horizontal[SGL_SIMD_BICUBIC_TAP_COUNT];
    int32x4_t value;

    col_lookup = &data->lut->col_lookup;
    row_lookup = &data->lut->row_lookup;
    src_rows[0] = &data->src[row_lookup->y1[row] * data->src_stride];
    src_rows[1] = &data->src[row_lookup->y2[row] * data->src_stride];
    src_rows[2] = &data->src[row_lookup->y3[row] * data->src_stride];
    src_rows[3] = &data->src[row_lookup->y4[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];
    q = vdupq_n_s32((sgl_int32_t)row_lookup->q[row]);

    for (col = 0; col < data->lut->d_width; ++col) {
        offsets[0] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x1[col]);
        offsets[1] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x2[col]);
        offsets[2] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x3[col]);
        offsets[3] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x4[col]);
        p = vdupq_n_s32((sgl_int32_t)col_lookup->p[col]);

        for (src_row = 0; src_row < SGL_SIMD_BICUBIC_TAP_COUNT; ++src_row) {
            horizontal[src_row] = sgl_neon_bicubic_interpolation(
                sgl_neon_bicubic_load_pixel_q11_bpp32(
                    &src_rows[src_row][offsets[0]]),
                sgl_neon_bicubic_load_pixel_q11_bpp32(
                    &src_rows[src_row][offsets[1]]),
                sgl_neon_bicubic_load_pixel_q11_bpp32(
                    &src_rows[src_row][offsets[2]]),
                sgl_neon_bicubic_load_pixel_q11_bpp32(
                    &src_rows[src_row][offsets[3]]),
                p);
        }

        value = sgl_neon_bicubic_interpolation(
            horizontal[0], horizontal[1], horizontal[2], horizontal[3], q);
        value = vrshrq_n_s32(value, SGL_Q11_FRAC_BITS);
        sgl_neon_bicubic_store_pixel_bpp32(dst, value);
        dst = &dst[SGL_BPP32];
    }
}

/*
 * Cached separable downscale bpp32 path
 * -------------------------------------
 * The fused pixel kernel repeats four horizontal cubic evaluations for every
 * destination row.  During downscale, neighboring four-row source windows
 * still overlap, so cache horizontal Q11 rows by source y:
 *
 *   source y ---- RGBA horizontal cubic ---- Q11 cache[y & mask] -+
 *                                                                  |
 *   four cached rows ---------------- contiguous RGBA loads --------+
 *                                                                  |
 *                                                       vertical cubic
 *                                                                  |
 *                                                             dst RGBA
 *
 * Each horizontal and vertical operation keeps RGBA in one four-lane vector.
 * The cache is private to a worker range, avoiding synchronization while
 * preserving reuse within that range.  Upscale continues to use its existing
 * table-based path.
 */
static SGL_ALWAYS_INLINE void sgl_simd_bicubic_horizontal_bpp32(
    const sgl_uint8_t *SGL_RESTRICT src_row,
    sgl_q11_ext_t *SGL_RESTRICT dst_row,
    const bicubic_column_lookup_t *SGL_RESTRICT col_lookup,
    sgl_int32_t d_width,
    sgl_int32_t src_stride)
{
    sgl_int32_t offsets[SGL_SIMD_BICUBIC_TAP_COUNT];
    sgl_int32_t col;
    int32x4_t p;
    int32x4_t value;

    for (col = 0; col < d_width; ++col) {
        offsets[0] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x1[col]);
        offsets[1] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x2[col]);
        offsets[2] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x3[col]);
        offsets[3] = SGL_RESIZE_BPP32_BYTE_OFFSET(col_lookup->x4[col]);
        p = vdupq_n_s32((sgl_int32_t)col_lookup->p[col]);
        sgl_resize_prefetch_source_read(
            src_row, offsets[0], src_stride, col);
        value = sgl_neon_bicubic_interpolation(
            sgl_neon_bicubic_load_pixel_q11_bpp32(&src_row[offsets[0]]),
            sgl_neon_bicubic_load_pixel_q11_bpp32(&src_row[offsets[1]]),
            sgl_neon_bicubic_load_pixel_q11_bpp32(&src_row[offsets[2]]),
            sgl_neon_bicubic_load_pixel_q11_bpp32(&src_row[offsets[3]]),
            p);
        vst1q_s32(&dst_row[SGL_RESIZE_BPP32_BYTE_OFFSET(col)], value);
    }
}

static SGL_ALWAYS_INLINE sgl_q11_ext_t *sgl_simd_bicubic_get_cached_row_bpp32(
    sgl_simd_bicubic_row_cache_t *SGL_RESTRICT cache,
    sgl_int32_t y,
    const sgl_bicubic_data_t *SGL_RESTRICT data)
{
    sgl_simd_bicubic_row_cache_t *slot;
    const sgl_uint8_t *src_row;

    /* Four clamped cubic taps span at most four adjacent y values. */
    slot = &cache[(sgl_uint32_t)y &
                  (sgl_uint32_t)SGL_SIMD_BICUBIC_ROW_CACHE_MASK];
    if (slot->y != y) {
        src_row = &data->src[y * data->src_stride];
        sgl_simd_bicubic_horizontal_bpp32(
            src_row,
            slot->row,
            &data->lut->col_lookup,
            data->lut->d_width,
            data->src_stride);
        slot->y = y;
    }

    return slot->row;
}

static SGL_ALWAYS_INLINE void sgl_simd_bicubic_vertical_bpp32(
    sgl_uint8_t *SGL_RESTRICT dst_row,
    const sgl_q11_ext_t *SGL_RESTRICT row1,
    const sgl_q11_ext_t *SGL_RESTRICT row2,
    const sgl_q11_ext_t *SGL_RESTRICT row3,
    const sgl_q11_ext_t *SGL_RESTRICT row4,
    sgl_int32_t q,
    sgl_int32_t d_width)
{
    sgl_int32_t col;
    sgl_int32_t offset;
    int32x4_t value;
    int32x4_t vec_q;

    vec_q = vdupq_n_s32(q);
    for (col = 0; col < d_width; ++col) {
        offset = SGL_RESIZE_BPP32_BYTE_OFFSET(col);
        value = sgl_neon_bicubic_interpolation(
            vld1q_s32(&row1[offset]),
            vld1q_s32(&row2[offset]),
            vld1q_s32(&row3[offset]),
            vld1q_s32(&row4[offset]),
            vec_q);
        value = vrshrq_n_s32(value, SGL_Q11_FRAC_BITS);
        sgl_neon_bicubic_store_pixel_bpp32(&dst_row[offset], value);
    }
}

static sgl_result_t sgl_simd_resize_bicubic_range_separable_bpp32(
    sgl_bicubic_data_t *SGL_RESTRICT data,
    sgl_int32_t start_row,
    sgl_int32_t row_count)
{
    sgl_result_t result;
    sgl_simd_bicubic_row_cache_t cache[SGL_SIMD_BICUBIC_ROW_CACHE_COUNT];
    sgl_q11_ext_t *row_storage;
    sgl_q11_ext_t *row1;
    sgl_q11_ext_t *row2;
    sgl_q11_ext_t *row3;
    sgl_q11_ext_t *row4;
    sgl_uint8_t *dst_row;
    sgl_int32_t row;
    sgl_int32_t end_row;
    sgl_int32_t row_width;
    sgl_int32_t slot;

    result = SGL_SUCCESS;
    row_storage = SGL_NULL;
    row_width = SGL_RESIZE_BPP32_BYTE_OFFSET(data->lut->d_width);
    end_row = start_row + row_count;
    if (end_row > data->lut->d_height) {
        end_row = data->lut->d_height;
    }

    row_storage = sgl_memory_as_q11_ext(sgl_malloc(
        sizeof(sgl_q11_ext_t) * (sgl_size_t)row_width *
        (sgl_size_t)SGL_SIMD_BICUBIC_ROW_CACHE_COUNT));
    if (row_storage != SGL_NULL) {
        for (slot = 0; slot < SGL_SIMD_BICUBIC_ROW_CACHE_COUNT; ++slot) {
            cache[slot].y = -1;
            cache[slot].row = &row_storage[slot * row_width];
        }

        for (row = start_row; row < end_row; ++row) {
            row1 = sgl_simd_bicubic_get_cached_row_bpp32(
                cache, data->lut->row_lookup.y1[row], data);
            row2 = sgl_simd_bicubic_get_cached_row_bpp32(
                cache, data->lut->row_lookup.y2[row], data);
            row3 = sgl_simd_bicubic_get_cached_row_bpp32(
                cache, data->lut->row_lookup.y3[row], data);
            row4 = sgl_simd_bicubic_get_cached_row_bpp32(
                cache, data->lut->row_lookup.y4[row], data);
            dst_row = &data->dst[row * data->dst_stride];
            sgl_simd_bicubic_vertical_bpp32(
                dst_row,
                row1,
                row2,
                row3,
                row4,
                (sgl_int32_t)data->lut->row_lookup.q[row],
                data->lut->d_width);
        }
    }
    else {
        result = SGL_ERROR_MEMORY_ALLOCATION;
    }

    SGL_SAFE_FREE(row_storage);

    return result;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bicubic_upscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_lanes, sgl_int32_t step, sgl_int32_t bpp,
                                    sgl_bicubic_data_t *data)
{
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t lane;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_int32_t x3_off;
    sgl_int32_t x4_off;
    const sgl_uint8_t *src_y1_buf;
    const sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y3_buf;
    const sgl_uint8_t *src_y4_buf;
    sgl_uint8_t *dst;

    uint8x8_t x1_col;
    uint8x8_t x2_col;
    uint8x8_t x3_col;
    uint8x8_t x4_col;

    sgl_simd_q11_t p;
    sgl_simd_q11_ext_t p_lo;
    sgl_simd_q11_ext_t p_hi;
    sgl_simd_q11_ext_t q;

    uint8x16x4_t vtbl4_y1x1;
    uint8x16x4_t vtbl4_y1x2;
    uint8x16x4_t vtbl4_y1x3;
    uint8x16x4_t vtbl4_y1x4;
    uint8x16x4_t vtbl4_y2x1;
    uint8x16x4_t vtbl4_y2x2;
    uint8x16x4_t vtbl4_y2x3;
    uint8x16x4_t vtbl4_y2x4;
    uint8x16x4_t vtbl4_y3x1;
    uint8x16x4_t vtbl4_y3x2;
    uint8x16x4_t vtbl4_y3x3;
    uint8x16x4_t vtbl4_y3x4;
    uint8x16x4_t vtbl4_y4x1;
    uint8x16x4_t vtbl4_y4x2;
    uint8x16x4_t vtbl4_y4x3;
    uint8x16x4_t vtbl4_y4x4;

    uint8x16x3_t vtbl3_y1x1;
    uint8x16x3_t vtbl3_y1x2;
    uint8x16x3_t vtbl3_y1x3;
    uint8x16x3_t vtbl3_y1x4;
    uint8x16x3_t vtbl3_y2x1;
    uint8x16x3_t vtbl3_y2x2;
    uint8x16x3_t vtbl3_y2x3;
    uint8x16x3_t vtbl3_y2x4;
    uint8x16x3_t vtbl3_y3x1;
    uint8x16x3_t vtbl3_y3x2;
    uint8x16x3_t vtbl3_y3x3;
    uint8x16x3_t vtbl3_y3x4;
    uint8x16x3_t vtbl3_y4x1;
    uint8x16x3_t vtbl3_y4x2;
    uint8x16x3_t vtbl3_y4x3;
    uint8x16x3_t vtbl3_y4x4;

    uint8x16x2_t vtbl2_y1x1;
    uint8x16x2_t vtbl2_y1x2;
    uint8x16x2_t vtbl2_y1x3;
    uint8x16x2_t vtbl2_y1x4;
    uint8x16x2_t vtbl2_y2x1;
    uint8x16x2_t vtbl2_y2x2;
    uint8x16x2_t vtbl2_y2x3;
    uint8x16x2_t vtbl2_y2x4;
    uint8x16x2_t vtbl2_y3x1;
    uint8x16x2_t vtbl2_y3x2;
    uint8x16x2_t vtbl2_y3x3;
    uint8x16x2_t vtbl2_y3x4;
    uint8x16x2_t vtbl2_y4x1;
    uint8x16x2_t vtbl2_y4x2;
    uint8x16x2_t vtbl2_y4x3;
    uint8x16x2_t vtbl2_y4x4;

    uint8x16_t   vtbl1_y1x1;
    uint8x16_t   vtbl1_y1x2;
    uint8x16_t   vtbl1_y1x3;
    uint8x16_t   vtbl1_y1x4;
    uint8x16_t   vtbl1_y2x1;
    uint8x16_t   vtbl1_y2x2;
    uint8x16_t   vtbl1_y2x3;
    uint8x16_t   vtbl1_y2x4;
    uint8x16_t   vtbl1_y3x1;
    uint8x16_t   vtbl1_y3x2;
    uint8x16_t   vtbl1_y3x3;
    uint8x16_t   vtbl1_y3x4;
    uint8x16_t   vtbl1_y4x1;
    uint8x16_t   vtbl1_y4x2;
    uint8x16_t   vtbl1_y4x3;
    uint8x16_t   vtbl1_y4x4;

    uint8x8_t x1;
    uint8x8_t x2;
    uint8x8_t x3;
    uint8x8_t x4;

    sgl_simd_q11_ext_t v1_lo;
    sgl_simd_q11_ext_t v2_lo;
    sgl_simd_q11_ext_t v3_lo;
    sgl_simd_q11_ext_t v4_lo;
    sgl_simd_q11_ext_t v1_hi;
    sgl_simd_q11_ext_t v2_hi;
    sgl_simd_q11_ext_t v3_hi;
    sgl_simd_q11_ext_t v4_hi;
    sgl_simd_q11_ext_t v_lo;
    sgl_simd_q11_ext_t v_hi;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;

    /* set 'row' data */
    q = vdupq_n_s32((sgl_int32_t)row_lookup->q[row]);

    src_y1_buf = &data->src[row_lookup->y1[row] * data->src_stride];
    src_y2_buf = &data->src[row_lookup->y2[row] * data->src_stride];
    src_y3_buf = &data->src[row_lookup->y3[row] * data->src_stride];
    src_y4_buf = &data->src[row_lookup->y4[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

    for (lane = 0; lane < num_lanes; ++lane) {
        col = NEON_LANE_OFFSET(lane);

        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        x3_off = col_lookup->x3[col] * bpp;
        x4_off = col_lookup->x4[col] * bpp;

        p = vld1q_s16(&col_lookup->p[col]);
        p_lo = vmovl_s16(vget_low_s16(p));
        p_hi = vmovl_high_s16(p);

        x1_col = sgl_neon_vld_col(&col_lookup->x1[col], col_lookup->x1[col]);
        x2_col = sgl_neon_vld_col(&col_lookup->x2[col], col_lookup->x2[col]);
        x3_col = sgl_neon_vld_col(&col_lookup->x3[col], col_lookup->x3[col]);
        x4_col = sgl_neon_vld_col(&col_lookup->x4[col], col_lookup->x4[col]);

        switch (bpp) {
        case SGL_BPP32:
            vtbl4_y1x1 = vld4q_u8(&src_y1_buf[x1_off]);
            vtbl4_y1x2 = vld4q_u8(&src_y1_buf[x2_off]);
            vtbl4_y1x3 = vld4q_u8(&src_y1_buf[x3_off]);
            vtbl4_y1x4 = vld4q_u8(&src_y1_buf[x4_off]);

            vtbl4_y2x1 = vld4q_u8(&src_y2_buf[x1_off]);
            vtbl4_y2x2 = vld4q_u8(&src_y2_buf[x2_off]);
            vtbl4_y2x3 = vld4q_u8(&src_y2_buf[x3_off]);
            vtbl4_y2x4 = vld4q_u8(&src_y2_buf[x4_off]);

            vtbl4_y3x1 = vld4q_u8(&src_y3_buf[x1_off]);
            vtbl4_y3x2 = vld4q_u8(&src_y3_buf[x2_off]);
            vtbl4_y3x3 = vld4q_u8(&src_y3_buf[x3_off]);
            vtbl4_y3x4 = vld4q_u8(&src_y3_buf[x4_off]);

            vtbl4_y4x1 = vld4q_u8(&src_y4_buf[x1_off]);
            vtbl4_y4x2 = vld4q_u8(&src_y4_buf[x2_off]);
            vtbl4_y4x3 = vld4q_u8(&src_y4_buf[x3_off]);
            vtbl4_y4x4 = vld4q_u8(&src_y4_buf[x4_off]);

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
            vtbl3_y1x1 = vld3q_u8(&src_y1_buf[x1_off]);
            vtbl3_y1x2 = vld3q_u8(&src_y1_buf[x2_off]);
            vtbl3_y1x3 = vld3q_u8(&src_y1_buf[x3_off]);
            vtbl3_y1x4 = vld3q_u8(&src_y1_buf[x4_off]);

            vtbl3_y2x1 = vld3q_u8(&src_y2_buf[x1_off]);
            vtbl3_y2x2 = vld3q_u8(&src_y2_buf[x2_off]);
            vtbl3_y2x3 = vld3q_u8(&src_y2_buf[x3_off]);
            vtbl3_y2x4 = vld3q_u8(&src_y2_buf[x4_off]);

            vtbl3_y3x1 = vld3q_u8(&src_y3_buf[x1_off]);
            vtbl3_y3x2 = vld3q_u8(&src_y3_buf[x2_off]);
            vtbl3_y3x3 = vld3q_u8(&src_y3_buf[x3_off]);
            vtbl3_y3x4 = vld3q_u8(&src_y3_buf[x4_off]);

            vtbl3_y4x1 = vld3q_u8(&src_y4_buf[x1_off]);
            vtbl3_y4x2 = vld3q_u8(&src_y4_buf[x2_off]);
            vtbl3_y4x3 = vld3q_u8(&src_y4_buf[x3_off]);
            vtbl3_y4x4 = vld3q_u8(&src_y4_buf[x4_off]);

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
            vtbl2_y1x1 = vld2q_u8(&src_y1_buf[x1_off]);
            vtbl2_y1x2 = vld2q_u8(&src_y1_buf[x2_off]);
            vtbl2_y1x3 = vld2q_u8(&src_y1_buf[x3_off]);
            vtbl2_y1x4 = vld2q_u8(&src_y1_buf[x4_off]);

            vtbl2_y2x1 = vld2q_u8(&src_y2_buf[x1_off]);
            vtbl2_y2x2 = vld2q_u8(&src_y2_buf[x2_off]);
            vtbl2_y2x3 = vld2q_u8(&src_y2_buf[x3_off]);
            vtbl2_y2x4 = vld2q_u8(&src_y2_buf[x4_off]);

            vtbl2_y3x1 = vld2q_u8(&src_y3_buf[x1_off]);
            vtbl2_y3x2 = vld2q_u8(&src_y3_buf[x2_off]);
            vtbl2_y3x3 = vld2q_u8(&src_y3_buf[x3_off]);
            vtbl2_y3x4 = vld2q_u8(&src_y3_buf[x4_off]);

            vtbl2_y4x1 = vld2q_u8(&src_y4_buf[x1_off]);
            vtbl2_y4x2 = vld2q_u8(&src_y4_buf[x2_off]);
            vtbl2_y4x3 = vld2q_u8(&src_y4_buf[x3_off]);
            vtbl2_y4x4 = vld2q_u8(&src_y4_buf[x4_off]);

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
            vtbl1_y1x1 = vld1q_u8(&src_y1_buf[x1_off]);
            vtbl1_y1x2 = vld1q_u8(&src_y1_buf[x2_off]);
            vtbl1_y1x3 = vld1q_u8(&src_y1_buf[x3_off]);
            vtbl1_y1x4 = vld1q_u8(&src_y1_buf[x4_off]);

            vtbl1_y2x1 = vld1q_u8(&src_y2_buf[x1_off]);
            vtbl1_y2x2 = vld1q_u8(&src_y2_buf[x2_off]);
            vtbl1_y2x3 = vld1q_u8(&src_y2_buf[x3_off]);
            vtbl1_y2x4 = vld1q_u8(&src_y2_buf[x4_off]);

            vtbl1_y3x1 = vld1q_u8(&src_y3_buf[x1_off]);
            vtbl1_y3x2 = vld1q_u8(&src_y3_buf[x2_off]);
            vtbl1_y3x3 = vld1q_u8(&src_y3_buf[x3_off]);
            vtbl1_y3x4 = vld1q_u8(&src_y3_buf[x4_off]);

            vtbl1_y4x1 = vld1q_u8(&src_y4_buf[x1_off]);
            vtbl1_y4x2 = vld1q_u8(&src_y4_buf[x2_off]);
            vtbl1_y4x3 = vld1q_u8(&src_y4_buf[x3_off]);
            vtbl1_y4x4 = vld1q_u8(&src_y4_buf[x4_off]);

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
        default:
            /* Unsupported bytes-per-pixel value. */
            break;
        }

        dst = &dst[step];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_bicubic_downscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_lanes, sgl_int32_t step, sgl_int32_t bpp,
                                    sgl_bicubic_data_t *data)
{
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t lane;
    const sgl_uint8_t *src_y1_buf;
    const sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y3_buf;
    const sgl_uint8_t *src_y4_buf;
    sgl_uint8_t *dst;

    sgl_simd_q11_t p;
    sgl_simd_q11_ext_t p_lo;
    sgl_simd_q11_ext_t p_hi;
    sgl_simd_q11_ext_t q;

    uint8x8_t x1;
    uint8x8_t x2;
    uint8x8_t x3;
    uint8x8_t x4;

    sgl_simd_q11_ext_t v1_lo;
    sgl_simd_q11_ext_t v2_lo;
    sgl_simd_q11_ext_t v3_lo;
    sgl_simd_q11_ext_t v4_lo;
    sgl_simd_q11_ext_t v1_hi;
    sgl_simd_q11_ext_t v2_hi;
    sgl_simd_q11_ext_t v3_hi;
    sgl_simd_q11_ext_t v4_hi;
    sgl_simd_q11_ext_t v_lo;
    sgl_simd_q11_ext_t v_hi;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1;

    /* set common data */
    row_lookup = &data->lut->row_lookup;
    col_lookup = &data->lut->col_lookup;

    /* set 'row' data */
    q = vdupq_n_s32((sgl_int32_t)row_lookup->q[row]);

    src_y1_buf = &data->src[row_lookup->y1[row] * data->src_stride];
    src_y2_buf = &data->src[row_lookup->y2[row] * data->src_stride];
    src_y3_buf = &data->src[row_lookup->y3[row] * data->src_stride];
    src_y4_buf = &data->src[row_lookup->y4[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

    for (lane = 0; lane < num_lanes; ++lane) {
        col = NEON_LANE_OFFSET(lane);
        p = vld1q_s16(&col_lookup->p[col]);
        p_lo = vmovl_s16(vget_low_s16(p));
        p_hi = vmovl_high_s16(p);

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
            default:
                /* Unsupported bytes-per-pixel value. */
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
        default:
            /* Unsupported bytes-per-pixel value. */
            break;
        }

        dst = &dst[step];
    }

    return dst;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_bicubic_line_stripe(sgl_int32_t row, sgl_bicubic_data_t *data) {
    bicubic_column_lookup_t *col_lookup;
    bicubic_row_lookup_t *row_lookup;
    sgl_int32_t col;
    sgl_int32_t d_width;
    sgl_int32_t bpp;
    sgl_int32_t step;
    sgl_int32_t x1_off;
    sgl_int32_t x2_off;
    sgl_int32_t x3_off;
    sgl_int32_t x4_off;
    sgl_int32_t y1;
    sgl_int32_t y2;
    sgl_int32_t y3;
    sgl_int32_t y4;
    sgl_q11_t p;
    sgl_q11_t q;
    sgl_q11_ext_t v1;
    sgl_q11_ext_t v2;
    sgl_q11_ext_t v3;
    sgl_q11_ext_t v4;
    sgl_q11_ext_t value;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_int32_t ch;
    sgl_int32_t src_stride;
    const sgl_uint8_t *src_y1_buf;
    const sgl_uint8_t *src_y2_buf;
    const sgl_uint8_t *src_y3_buf;
    const sgl_uint8_t *src_y4_buf;
    const sgl_uint8_t *src_y1x1;
    const sgl_uint8_t *src_y1x2;
    const sgl_uint8_t *src_y1x3;
    const sgl_uint8_t *src_y1x4;
    const sgl_uint8_t *src_y2x1;
    const sgl_uint8_t *src_y2x2;
    const sgl_uint8_t *src_y2x3;
    const sgl_uint8_t *src_y2x4;
    const sgl_uint8_t *src_y3x1;
    const sgl_uint8_t *src_y3x2;
    const sgl_uint8_t *src_y3x3;
    const sgl_uint8_t *src_y3x4;
    const sgl_uint8_t *src_y4x1;
    const sgl_uint8_t *src_y4x2;
    const sgl_uint8_t *src_y4x3;
    const sgl_uint8_t *src_y4x4;

    sgl_int32_t num_lanes;
    sgl_int32_t tail_col;

    d_width = data->lut->d_width;
    bpp = data->bpp;
    num_lanes = NEON_LANE_COUNT(d_width);
    step = NEON_LANE_BYTE_STEP(bpp);

    if (data->src_stride <= data->dst_stride) {
        dst = sgl_simd_resize_bicubic_upscale_line_stripe(row, num_lanes, step, bpp, data);
        tail_col = NEON_LANE_OFFSET(num_lanes);
    }
    else {
        if (bpp == SGL_BPP32) {
            sgl_simd_resize_bicubic_downscale_line_bpp32(row, data);
            dst = &data->dst[row * data->dst_stride];
            tail_col = d_width;
        }
        else {
            dst = sgl_simd_resize_bicubic_downscale_line_stripe(
                row, num_lanes, step, bpp, data);
            tail_col = NEON_LANE_OFFSET(num_lanes);
        }
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
    src_y1_buf = &src[y1 * src_stride];
    src_y2_buf = &src[y2 * src_stride];
    src_y3_buf = &src[y3 * src_stride];
    src_y4_buf = &src[y4 * src_stride];

    for (col = tail_col; col < d_width; ++col) {
        x1_off = col_lookup->x1[col] * bpp;
        x2_off = col_lookup->x2[col] * bpp;
        x3_off = col_lookup->x3[col] * bpp;
        x4_off = col_lookup->x4[col] * bpp;
        p = col_lookup->p[col];

        src_y1x1 = &src_y1_buf[x1_off];
        src_y1x2 = &src_y1_buf[x2_off];
        src_y1x3 = &src_y1_buf[x3_off];
        src_y1x4 = &src_y1_buf[x4_off];

        src_y2x1 = &src_y2_buf[x1_off];
        src_y2x2 = &src_y2_buf[x2_off];
        src_y2x3 = &src_y2_buf[x3_off];
        src_y2x4 = &src_y2_buf[x4_off];

        src_y3x1 = &src_y3_buf[x1_off];
        src_y3x2 = &src_y3_buf[x2_off];
        src_y3x3 = &src_y3_buf[x3_off];
        src_y3x4 = &src_y3_buf[x4_off];

        src_y4x1 = &src_y4_buf[x1_off];
        src_y4x2 = &src_y4_buf[x2_off];
        src_y4x3 = &src_y4_buf[x3_off];
        src_y4x4 = &src_y4_buf[x4_off];

        for (ch = 0; ch < bpp; ++ch) {
            v1 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y1x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y1x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y1x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y1x4[ch]), (sgl_q11_ext_t)p);
            v2 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y2x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y2x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y2x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y2x4[ch]), (sgl_q11_ext_t)p);
            v3 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y3x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y3x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y3x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y3x4[ch]), (sgl_q11_ext_t)p);
            v4 = sgl_generic_bicubic_interpolation(sgl_int_to_q11((sgl_q11_ext_t)src_y4x1[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y4x2[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y4x3[ch]), sgl_int_to_q11((sgl_q11_ext_t)src_y4x4[ch]), (sgl_q11_ext_t)p);
            value = sgl_generic_bicubic_interpolation(v1, v2, v3, v4, (sgl_q11_ext_t)q);
            value = sgl_q11_shift_down(sgl_q11_round_up(value));

            /* Q11 -> u8 */
            dst[ch] = sgl_clamp_u8_i32(value);
        }
        dst = &dst[bpp];
    }
}

static sgl_int32_t sgl_simd_resize_bicubic_count_errors(
                const sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                const sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_int32_t errcnt = 0;

    /* Check buffer address. */
    if ((dst == SGL_NULL) || (src == SGL_NULL)) {
        errcnt += 1;
    }

    /* Check boundary. */
    if ((d_width <= 0) || (d_height <= 0) || (s_width <= 0) || (s_height <= 0)) {
        errcnt += 1;
    }

    /* Check bytes per pixel. */
    if (bpp <= 0) {
        errcnt += 1;
    }

    return errcnt;
}

static sgl_bool_t sgl_simd_resize_bicubic_is_same_size(
                sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_bool_t result = SGL_FALSE;

    if ((d_width == s_width) && (d_height == s_height)) {
        result = SGL_TRUE;
    }

    return result;
}

static void sgl_simd_resize_bicubic_copy_same_size(
                sgl_uint8_t *SGL_RESTRICT dst,
                const sgl_uint8_t *SGL_RESTRICT src,
                sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t bpp)
{
    (void)sgl_memcpy(dst, src, (sgl_size_t)d_width * (sgl_size_t)d_height * (sgl_size_t)bpp);
}

static sgl_bicubic_lookup_t *sgl_simd_resize_bicubic_select_lut(
                sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
                sgl_bicubic_lookup_t **SGL_RESTRICT temp_lut,
                sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_bicubic_lookup_t *lut = SGL_NULL;

    if (ext_lut != SGL_NULL) {
        if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
            (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
        {
            /* Apply external look-up table. */
            lut = ext_lut;
        }
    }

    if (lut == SGL_NULL) {
        /* Create temp look-up table. */
        *temp_lut = sgl_generic_create_bicubic_lut(d_width, d_height, s_width, s_height);
        lut = *temp_lut;
    }

    return lut;
}

static void sgl_simd_resize_bicubic_set_data(
                sgl_bicubic_data_t *SGL_RESTRICT data,
                sgl_bicubic_lookup_t *SGL_RESTRICT lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width,
                sgl_int32_t bpp)
{
    data->bpp = bpp;
    data->src = src;
    data->dst = dst;
    data->lut = lut;
    data->src_stride = s_width * bpp;
    data->dst_stride = d_width * bpp;
}

static void sgl_simd_resize_bicubic_single(sgl_bicubic_data_t *SGL_RESTRICT data, sgl_int32_t d_height)
{
    sgl_result_t result;
    sgl_int32_t row;

    result = SGL_ERROR_NOT_SUPPORTED;
    if ((data->bpp == SGL_BPP32) &&
        (data->src_stride > data->dst_stride))
    {
        result = sgl_simd_resize_bicubic_range_separable_bpp32(
            data, 0, d_height);
    }

    if (result != SGL_SUCCESS) {
        for (row = 0; row < d_height; ++row) {
            sgl_simd_resize_bicubic_line_stripe(row, data);
        }
    }
}

#if defined(SGL_CFG_HAS_THREAD)
static sgl_result_t sgl_simd_resize_bicubic_threaded(
                sgl_threadpool_t *SGL_RESTRICT pool,
                sgl_bicubic_data_t *SGL_RESTRICT data,
                sgl_int32_t d_height)
{
    sgl_result_t result = SGL_ERROR_MEMORY_ALLOCATION;
    sgl_bicubic_current_t *currents;
    sgl_queue_t *operations = SGL_NULL;
    sgl_int32_t i;
    sgl_int32_t num_operations;
    sgl_int32_t mod_operations;
    sgl_int32_t bulk_size;
    sgl_int32_t minimum_bulk;

    minimum_bulk = SGL_SIMD_BULK_SIZE;
    if ((data->bpp == SGL_BPP32) &&
        (data->src_stride > data->dst_stride))
    {
        minimum_bulk = SGL_SIMD_BICUBIC_CACHE_BULK_SIZE;
    }
    bulk_size = sgl_resize_thread_bulk_size(
        pool, d_height, minimum_bulk);
    num_operations = d_height / bulk_size;
    mod_operations = d_height % bulk_size;
    if (mod_operations != 0) {
        num_operations += 1;
    }

    operations = sgl_queue_create((sgl_size_t)num_operations);
    currents = sgl_memory_as_bicubic_current(
        sgl_malloc(sizeof(sgl_bicubic_current_t) * (sgl_size_t)num_operations));
    if ((operations != SGL_NULL) && (currents != SGL_NULL)) {
        for (i = 0; i < num_operations; ++i) {
            currents[i].row = i * bulk_size;
            currents[i].count = bulk_size;
            (void)sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
        }

        if (mod_operations != 0) {
            currents[num_operations - 1].count = mod_operations;
        }

        /* Multi-threaded resize. */
        result = sgl_threadpool_attach_routine_consuming(
            pool, sgl_simd_resize_bicubic_routine, operations, (void *)data);
        sgl_queue_destroy(&operations);
    }
    SGL_SAFE_FREE(currents);
    SGL_SAFE_FREE(operations);

    return result;
}
#endif  /* !SGL_CFG_HAS_THREAD */

static sgl_result_t sgl_simd_resize_bicubic_run(
                sgl_threadpool_t *SGL_RESTRICT pool,
                sgl_bicubic_data_t *SGL_RESTRICT data,
                sgl_int32_t d_height)
{
    sgl_result_t result = SGL_SUCCESS;

    if (pool == SGL_NULL) {
        /* Single-threaded resize. */
        sgl_simd_resize_bicubic_single(data, d_height);
    }
#if defined(SGL_CFG_HAS_THREAD)
    else {
        result = sgl_simd_resize_bicubic_threaded(pool, data, d_height);
    }
#else
    else {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
#endif  /* !SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_simd_resize_bicubic(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bicubic_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_bicubic_data_t data;
    sgl_bicubic_lookup_t *lut = SGL_NULL;
    sgl_bicubic_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;

    SGL_TRACE_RESIZE_BEGIN(
        SGL_TRACE_BACKEND_SIMD,
        SGL_TRACE_METHOD_BICUBIC,
        d_width,
        d_height,
        s_width,
        s_height,
        bpp,
        SGL_TRACE_REQUESTED_THREADS(pool),
        (ext_lut != SGL_NULL));
    errcnt = sgl_simd_resize_bicubic_count_errors(dst, d_width, d_height, src, s_width, s_height, bpp);

    if (errcnt == 0) {
        if (sgl_simd_resize_bicubic_is_same_size(d_width, d_height, s_width, s_height) == SGL_TRUE) {
            sgl_simd_resize_bicubic_copy_same_size(dst, src, d_width, d_height, bpp);
        }
        else {
            lut = sgl_simd_resize_bicubic_select_lut(ext_lut, &temp_lut, d_width, d_height, s_width, s_height);
        }

        if (lut != SGL_NULL) {
            sgl_simd_resize_bicubic_set_data(&data, lut, dst, d_width, src, s_width, bpp);
            result = sgl_simd_resize_bicubic_run(pool, &data, d_height);
        }

        if (temp_lut != SGL_NULL) {
            /* Destroy temp look-up table. */
            sgl_generic_destroy_bicubic_lut(temp_lut);
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    SGL_TRACE_RESIZE_END(
        SGL_TRACE_BACKEND_SIMD, SGL_TRACE_METHOD_BICUBIC, result);

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_bicubic_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_bicubic_current_t *cur = sgl_memory_as_const_bicubic_current(current);
    sgl_bicubic_data_t *data = sgl_memory_as_bicubic_data(cookie);
    sgl_result_t result;
    sgl_int32_t row;

    result = SGL_ERROR_NOT_SUPPORTED;
    if ((data->bpp == SGL_BPP32) &&
        (data->src_stride > data->dst_stride))
    {
        result = sgl_simd_resize_bicubic_range_separable_bpp32(
            data, cur->row, cur->count);
    }

    if (result != SGL_SUCCESS) {
        for (row = cur->row; row < (cur->row + cur->count); ++row) {
            sgl_simd_resize_bicubic_line_stripe(row, data);
        }
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
