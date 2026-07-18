/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "nearest_neighbor.h"
#include "resize_bitops.h"
#include "sgl_trace.h"
#include "threaded_resize.h"

#define NEON_LANE_SHIFT                  (4U)
#define NEON_HALF_LANE_SHIFT             (3U)
#define NEON_LANE_SIZE                   (16)
#define NEON_HALF_LANE_SIZE              (8)
#define NEON_SOURCE_TABLE_PIXELS          (16)
#define NEON_LANE_OFFSET(lane) \
    sgl_simd_nearest_shift_left_nonnegative((lane), NEON_LANE_SHIFT)
#define NEON_HALF_LANE_OFFSET(lane) \
    sgl_simd_nearest_shift_left_nonnegative((lane), NEON_HALF_LANE_SHIFT)
#define NEON_LANE_COUNT(width) \
    sgl_simd_nearest_shift_right_nonnegative((width), NEON_LANE_SHIFT)
#define NEON_HALF_LANE_COUNT(width) \
    sgl_simd_nearest_shift_right_nonnegative((width), NEON_HALF_LANE_SHIFT)
#define NEON_HALF_LANE_BYTE_STEP(bpp) \
    sgl_simd_nearest_shift_left_nonnegative((bpp), NEON_HALF_LANE_SHIFT)
#define NEON_BPP32_TABLE_PIXELS           (8)
#define NEON_BPP32_TABLE_BYTE_STEP        (32)
#define NEON_BPP32_COMPONENT_INDEX_SCALE  (0x04040404U)
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define NEON_BPP32_COMPONENT_INDEX_BASE   (0x00010203U)
#else
#define NEON_BPP32_COMPONENT_INDEX_BASE   (0x03020100U)
#endif

/* Resize dimensions and loop indices are validated as nonnegative. */
static SGL_ALWAYS_INLINE sgl_int32_t sgl_simd_nearest_shift_left_nonnegative(
    sgl_int32_t value,
    sgl_uint32_t shift)
{
    sgl_int32_t result;

    /* cppcheck-suppress misra-c2012-10.8 */
    result = (sgl_int32_t)((sgl_uint32_t)value << shift);

    return result;
}

static SGL_ALWAYS_INLINE sgl_int32_t sgl_simd_nearest_shift_right_nonnegative(
    sgl_int32_t value,
    sgl_uint32_t shift)
{
    sgl_int32_t result;

    /* cppcheck-suppress misra-c2012-10.8 */
    result = (sgl_int32_t)((sgl_uint32_t)value >> shift);

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

#if defined(SGL_CFG_IS_ARM64)
static SGL_ALWAYS_INLINE sgl_uint32_t sgl_simd_nearest_pack_u8x4(
    sgl_uint8_t value0,
    sgl_uint8_t value1,
    sgl_uint8_t value2,
    sgl_uint8_t value3)
{
    sgl_uint32_t packed;

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    packed = ((sgl_uint32_t)value0 << 24U) |
             ((sgl_uint32_t)value1 << 16U) |
             ((sgl_uint32_t)value2 << 8U) |
             (sgl_uint32_t)value3;
#else
    packed = (sgl_uint32_t)value0 |
             ((sgl_uint32_t)value1 << 8U) |
             ((sgl_uint32_t)value2 << 16U) |
             ((sgl_uint32_t)value3 << 24U);
#endif

    return packed;
}
#endif  /* SGL_CFG_IS_ARM64 */

/*
 * AArch64 1-channel gather
 * ------------------------
 * NEON has no arbitrary byte gather.  Building one vector with sixteen lane
 * inserts creates a sixteen-instruction dependency chain.  Pack four groups
 * independently and insert four 32-bit lanes instead:
 *
 *   4 source bytes -> word 0 --+
 *   4 source bytes -> word 1 --+--> uint32x4_t --> uint8x16_t
 *   4 source bytes -> word 2 --+
 *   4 source bytes -> word 3 --+
 *
 * Adjacent LUT entries are loaded with paired scalar loads by AArch64
 * compilers, while the four pack chains can execute independently.
 */
static SGL_ALWAYS_INLINE uint8x16_t sgl_simd_nearest_gather_bpp8(
    const sgl_uint8_t *SGL_RESTRICT src,
    const sgl_int32_t *SGL_RESTRICT x)
{
    uint8x16_t gathered;

#if defined(SGL_CFG_IS_ARM64)
    uint32x4_t packed;
    sgl_uint32_t word0;
    sgl_uint32_t word1;
    sgl_uint32_t word2;
    sgl_uint32_t word3;

    word0 = sgl_simd_nearest_pack_u8x4(
        src[x[0]], src[x[1]], src[x[2]], src[x[3]]);
    word1 = sgl_simd_nearest_pack_u8x4(
        src[x[4]], src[x[5]], src[x[6]], src[x[7]]);
    word2 = sgl_simd_nearest_pack_u8x4(
        src[x[8]], src[x[9]], src[x[10]], src[x[11]]);
    word3 = sgl_simd_nearest_pack_u8x4(
        src[x[12]], src[x[13]], src[x[14]], src[x[15]]);

    packed = vdupq_n_u32(0U);
    packed = vsetq_lane_u32(word0, packed, 0);
    packed = vsetq_lane_u32(word1, packed, 1);
    packed = vsetq_lane_u32(word2, packed, 2);
    packed = vsetq_lane_u32(word3, packed, 3);
    gathered = vreinterpretq_u8_u32(packed);
#else
    gathered = vdupq_n_u8(0U);
    gathered = vsetq_lane_u8(src[x[0]], gathered, 0);
    gathered = vsetq_lane_u8(src[x[1]], gathered, 1);
    gathered = vsetq_lane_u8(src[x[2]], gathered, 2);
    gathered = vsetq_lane_u8(src[x[3]], gathered, 3);
    gathered = vsetq_lane_u8(src[x[4]], gathered, 4);
    gathered = vsetq_lane_u8(src[x[5]], gathered, 5);
    gathered = vsetq_lane_u8(src[x[6]], gathered, 6);
    gathered = vsetq_lane_u8(src[x[7]], gathered, 7);
    gathered = vsetq_lane_u8(src[x[8]], gathered, 8);
    gathered = vsetq_lane_u8(src[x[9]], gathered, 9);
    gathered = vsetq_lane_u8(src[x[10]], gathered, 10);
    gathered = vsetq_lane_u8(src[x[11]], gathered, 11);
    gathered = vsetq_lane_u8(src[x[12]], gathered, 12);
    gathered = vsetq_lane_u8(src[x[13]], gathered, 13);
    gathered = vsetq_lane_u8(src[x[14]], gathered, 14);
    gathered = vsetq_lane_u8(src[x[15]], gathered, 15);
#endif  /* SGL_CFG_IS_ARM64 */

    return gathered;
}

/*
 * Packed RGBA table indices
 * -------------------------
 * Four 32-bit LUT coordinates are converted directly into sixteen byte table
 * indices.  Multiplying a relative pixel coordinate by 0x04040404 replicates
 * its four-byte source offset across the word; adding 0x03020100 appends the
 * component offsets without scalar lane insertion.
 *
 *   [x0, x1, x2, x3] - base
 *             |
 *             v
 *   0x04040404 * relative + 0x03020100
 *             |
 *             v
 *   [4*x0+0, 4*x0+1, ... 4*x3+3] ---> packed-byte tbl indices
 */
static SGL_ALWAYS_INLINE uint8x16_t sgl_simd_nearest_bpp32_table_indices(
    const sgl_int32_t *SGL_RESTRICT x,
    sgl_int32_t table_base)
{
    int32x4_t coordinates;
    uint32x4_t relative;
    uint32x4_t indices;

    coordinates = vld1q_s32(x);
    relative = vreinterpretq_u32_s32(vsubq_s32(
        coordinates, vdupq_n_s32(table_base)));
    indices = vmlaq_n_u32(
        vdupq_n_u32(NEON_BPP32_COMPONENT_INDEX_BASE),
        relative,
        NEON_BPP32_COMPONENT_INDEX_SCALE);

    return vreinterpretq_u8_u32(indices);
}

static SGL_ALWAYS_INLINE const sgl_uint32_t *
sgl_simd_nearest_bpp32_load_address(const sgl_uint8_t *address)
{
    const sgl_uint32_t *result;

    /* BPP32 rows and pixel offsets are 32-bit aligned. */
    /* cppcheck-suppress misra-c2012-11.3 */
    result = (const sgl_uint32_t *)address;

    return result;
}

/*
 * 4-channel nearest-neighbor upscale
 * ----------------------------------
 * Eight output pixels can reference at most eight adjacent source pixels.
 * Keep those pixels packed, then select two groups of four RGBA pixels from a
 * 32-byte table.  This removes the previous deinterleave/reinterleave path:
 *
 *   previous: 64-byte ld4 -> four tbl -> st4
 *   current : 32-byte ld1 -> two tbl  -> two contiguous 16-byte stores
 *
 * The source table base is clamped so the final destination group never reads
 * beyond the source row.
 */
static SGL_ALWAYS_INLINE sgl_uint8_t *
sgl_simd_resize_nearest_neighbor_upscale_line_bpp32(
    sgl_int32_t row,
    sgl_int32_t num_tables,
    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut;
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    sgl_uint8_t *dst;
    sgl_int32_t col;
    sgl_int32_t lane;
    sgl_int32_t max_table_base;
    sgl_int32_t table_base;
    uint8x16x2_t source_table;
    uint8x16_t value0;
    uint8x16_t value1;

    lut = data->lut;
    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];
    max_table_base = lut->s_width - NEON_BPP32_TABLE_PIXELS;

    for (lane = 0; lane < num_tables; ++lane) {
        col = NEON_HALF_LANE_OFFSET(lane);
        table_base = x[col];
        if (table_base > max_table_base) {
            table_base = max_table_base;
        }

        source_table = vld1q_u8_x2(&src_y_buf[
            SGL_RESIZE_BPP32_BYTE_OFFSET(table_base)]);
        value0 = vqtbl2q_u8(
            source_table,
            sgl_simd_nearest_bpp32_table_indices(&x[col], table_base));
        value1 = vqtbl2q_u8(
            source_table,
            sgl_simd_nearest_bpp32_table_indices(
                &x[col + SGL_BPP32], table_base));
        vst1q_u8(dst, value0);
        vst1q_u8(&dst[NEON_LANE_SIZE], value1);
        dst = &dst[NEON_BPP32_TABLE_BYTE_STEP];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_nearest_neighbor_upscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_half_lanes, sgl_int32_t half_step, sgl_int32_t bpp,
                                    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    sgl_int32_t col;
    sgl_int32_t lane;
    sgl_int32_t max_table_base;
    sgl_int32_t table_base;
    sgl_int32_t x_col_base;
    const sgl_int32_t *x;
    sgl_uint8_t *src_y_buf;
    sgl_uint8_t *dst;

    uint8x8_t vec_x_col;
    uint8x16x4_t vtbl4_src;
    uint8x16x3_t vtbl3_src;
    uint8x16x2_t vtbl2_src;
    uint8x16_t vtbl1_src;

    uint8x8x4_t value4;
    uint8x8x3_t value3;
    uint8x8x2_t value2;
    uint8x8_t value1;

    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];
    vec_x_col = vdup_n_u8(0U);
    max_table_base = lut->s_width - NEON_SOURCE_TABLE_PIXELS;

    for (lane = 0; lane < num_half_lanes; ++lane) {
        col = NEON_HALF_LANE_OFFSET(lane);
        table_base = x[col];
        if (table_base > max_table_base) {
            table_base = max_table_base;
        }
        x_col_base = table_base;

        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 0);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 1);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 2);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 3);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 4);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 5);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 6);
        vec_x_col = vset_lane_u8(x[col++] - x_col_base, vec_x_col, 7);

        switch (bpp) {
        case SGL_BPP32:
            vtbl4_src = vld4q_u8(&src_y_buf[
                SGL_RESIZE_BPP32_BYTE_OFFSET(table_base)]);
            value4.val[3] = vqtbl1_u8(vtbl4_src.val[3], vec_x_col);
            value4.val[2] = vqtbl1_u8(vtbl4_src.val[2], vec_x_col);
            value4.val[1] = vqtbl1_u8(vtbl4_src.val[1], vec_x_col);
            value4.val[0] = vqtbl1_u8(vtbl4_src.val[0], vec_x_col);
            vst4_u8(dst, value4);
            break;
        case SGL_BPP24:
            vtbl3_src = vld3q_u8(&src_y_buf[
                table_base * SGL_BPP24]);
            value3.val[2] = vqtbl1_u8(vtbl3_src.val[2], vec_x_col);
            value3.val[1] = vqtbl1_u8(vtbl3_src.val[1], vec_x_col);
            value3.val[0] = vqtbl1_u8(vtbl3_src.val[0], vec_x_col);
            vst3_u8(dst, value3);
            break;
        case SGL_BPP16:
            vtbl2_src = vld2q_u8(&src_y_buf[
                SGL_RESIZE_BPP16_BYTE_OFFSET(table_base)]);
            value2.val[1] = vqtbl1_u8(vtbl2_src.val[1], vec_x_col);
            value2.val[0] = vqtbl1_u8(vtbl2_src.val[0], vec_x_col);
            vst2_u8(dst, value2);
            break;
        case SGL_BPP8:
            vtbl1_src = vld1q_u8(&src_y_buf[table_base]);
            value1 = vqtbl1_u8(vtbl1_src, vec_x_col);
            vst1_u8(dst, value1);
            break;
        default:
            /* Not Supported */
            break;
        }

        dst = &dst[half_step];
    }

    return dst;
}

/*
 * AArch64 has no vector gather instruction, so each source pixel still needs
 * an independent lane load.  Two vectors keep the load chains independent and
 * replace eight destination word stores with one paired vector store.
 *
 *   eight LUT coordinates ---> eight 32-bit lane loads ---> stp q0, q1
 */
static SGL_ALWAYS_INLINE sgl_uint8_t *
sgl_simd_resize_nearest_neighbor_downscale_line_bpp32(
    sgl_int32_t row,
    sgl_int32_t num_tables,
    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut;
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    sgl_uint8_t *dst;
    sgl_int32_t col;
    sgl_int32_t lane;
    uint32x4_t value0;
    uint32x4_t value1;

    lut = data->lut;
    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

    for (lane = 0; lane < num_tables; ++lane) {
        col = NEON_HALF_LANE_OFFSET(lane);
        value0 = vdupq_n_u32(0U);
        value1 = vdupq_n_u32(0U);
        value0 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col])]), value0, 0);
        value1 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 4])]), value1, 0);
        value0 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 1])]), value0, 1);
        value1 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 5])]), value1, 1);
        value0 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 2])]), value0, 2);
        value1 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 6])]), value1, 2);
        value0 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 3])]), value0, 3);
        value1 = vld1q_lane_u32(sgl_simd_nearest_bpp32_load_address(
            &src_y_buf[SGL_RESIZE_BPP32_BYTE_OFFSET(x[col + 7])]), value1, 3);
        vst1q_u8(dst, vreinterpretq_u8_u32(value0));
        vst1q_u8(&dst[NEON_LANE_SIZE], vreinterpretq_u8_u32(value1));
        dst = &dst[NEON_BPP32_TABLE_BYTE_STEP];
    }

    return dst;
}

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_nearest_neighbor_downscale_line_bpp8(
    sgl_int32_t row,
    sgl_int32_t num_lanes,
    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    sgl_int32_t col;
    sgl_int32_t lane;
    const sgl_int32_t *x;
    const sgl_uint8_t *src_y_buf;
    sgl_uint8_t *dst;
    uint8x16_t value1;

    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

    for (lane = 0; lane < num_lanes; ++lane) {
        col = NEON_LANE_OFFSET(lane);
        value1 = sgl_simd_nearest_gather_bpp8(src_y_buf, &x[col]);
        vst1q_u8(dst, value1);
        dst = &dst[NEON_LANE_SIZE];
    }

    return dst;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_nearest_neighbor_line_stripe(sgl_int32_t row, sgl_nearest_neighbor_data_t *data) {
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t d_width;
    sgl_int32_t bpp;
    sgl_int32_t step;
    sgl_int32_t num_lanes;
    sgl_int32_t lane_size;
    const sgl_int32_t *x;
    sgl_uint8_t *src_y_buf;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;

    d_width = lut->d_width;
    bpp = data->bpp;

    if (data->src_stride <= data->dst_stride) {
        num_lanes = NEON_HALF_LANE_COUNT(d_width);
        step = NEON_HALF_LANE_BYTE_STEP(bpp);
        lane_size = NEON_HALF_LANE_SIZE;
        if ((bpp == SGL_BPP32) &&
            (lut->s_width >= NEON_BPP32_TABLE_PIXELS)) {
            dst = sgl_simd_resize_nearest_neighbor_upscale_line_bpp32(
                row, num_lanes, data);
        }
        else {
            dst = sgl_simd_resize_nearest_neighbor_upscale_line_stripe(
                row, num_lanes, step, bpp, data);
        }
    }
    else {
        if (bpp == SGL_BPP32) {
            num_lanes = NEON_HALF_LANE_COUNT(d_width);
            lane_size = NEON_HALF_LANE_SIZE;
            dst = sgl_simd_resize_nearest_neighbor_downscale_line_bpp32(
                row, num_lanes, data);
        }
        else {
            num_lanes = NEON_LANE_COUNT(d_width);
            lane_size = NEON_LANE_SIZE;
            dst = sgl_simd_resize_nearest_neighbor_downscale_line_bpp8(
                row, num_lanes, data);
        }
    }

    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];

    for (col = num_lanes * lane_size; col < d_width; ++col) {
        src = &src_y_buf[x[col] * bpp];
        switch (bpp) {
        case SGL_BPP32:
            dst[3] = src[3];
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        case SGL_BPP24:
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        case SGL_BPP16:
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        case SGL_BPP8:
            dst[0] = src[0];
            break;
        default:
            for (ch = 0; ch < bpp; ++ch) {
                dst[ch] = src[ch];
            }
            break;
        }
        dst = &dst[bpp];
    }
}

static SGL_ALWAYS_INLINE sgl_bool_t sgl_simd_resize_nearest_uses_packed_range(
    const sgl_nearest_neighbor_data_t *data)
{
    sgl_bool_t result;

    result = SGL_FALSE;
    if ((data->src_stride <= data->dst_stride) &&
        (data->lut->s_width < NEON_SOURCE_TABLE_PIXELS))
    {
        result = SGL_TRUE;
    }
    else if ((data->src_stride > data->dst_stride) &&
        (data->bpp != SGL_BPP8) &&
        (data->bpp != SGL_BPP32))
    {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_int32_t sgl_simd_resize_nearest_count_errors(
    const sgl_uint8_t *dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    const sgl_uint8_t *src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp)
{
    sgl_int32_t errcnt;

    errcnt = 0;

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

    return errcnt;
}

static SGL_ALWAYS_INLINE sgl_bool_t sgl_simd_resize_nearest_is_same_size(
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_bool_t result;

    result = SGL_FALSE;
    if ((d_width == s_width) && (d_height == s_height)) {
        result = SGL_TRUE;
    }

    return result;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_nearest_copy_same_size(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t bpp)
{
    sgl_size_t copy_size;

    copy_size = (sgl_size_t)d_width * (sgl_size_t)d_height * (sgl_size_t)bpp;
    if (dst != src) {
        (void)sgl_memcpy(dst, src, copy_size);
    }
}

static sgl_nearest_neighbor_lookup_t *sgl_simd_resize_nearest_select_lut(
    sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
    sgl_nearest_neighbor_lookup_t **temp_lut,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_nearest_neighbor_lookup_t *lut;

    lut = SGL_NULL;
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
        *temp_lut = sgl_generic_create_nearest_neighbor_lut(
            d_width, d_height, s_width, s_height);
        lut = *temp_lut;
    }

    return lut;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_nearest_set_data(
    sgl_nearest_neighbor_data_t *data,
    sgl_nearest_neighbor_lookup_t *lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t d_width,
    sgl_int32_t bpp)
{
    data->bpp = bpp;
    data->src = src;
    data->dst = dst;
    data->lut = lut;
    data->src_stride = s_width * bpp;
    data->dst_stride = d_width * bpp;
}

static SGL_ALWAYS_INLINE void sgl_simd_resize_nearest_single(
    sgl_nearest_neighbor_data_t *data,
    sgl_int32_t d_height)
{
    sgl_int32_t row;

    if (sgl_simd_resize_nearest_uses_packed_range(data) == SGL_TRUE) {
        sgl_resize_nearest_neighbor_dispatch_packed_range(
            0, d_height, data);
    }
    else {
        for (row = 0; row < d_height; ++row) {
            sgl_simd_resize_nearest_neighbor_line_stripe(row, data);
        }
    }
}

#if defined(SGL_CFG_HAS_THREAD)
static sgl_result_t sgl_simd_resize_nearest_threaded(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_nearest_neighbor_data_t *data,
    sgl_int32_t d_height)
{
    sgl_result_t result;
    sgl_nearest_neighbor_current_t *currents;
    sgl_queue_t *operations;
    sgl_int32_t i;
    sgl_int32_t num_operations;
    sgl_int32_t mod_operations;
    sgl_int32_t bulk_size;

    result = SGL_ERROR_MEMORY_ALLOCATION;
    currents = SGL_NULL;
    operations = SGL_NULL;
    bulk_size = sgl_resize_uniform_thread_bulk_size(
        pool, d_height, SGL_SIMD_BULK_SIZE);
    num_operations = d_height / bulk_size;
    mod_operations = d_height % bulk_size;
    if (mod_operations != 0) {
        num_operations += 1;
    }

    operations = sgl_queue_create((sgl_size_t)num_operations);
    currents = sgl_memory_as_nearest_neighbor_current(sgl_malloc(
        sizeof(sgl_nearest_neighbor_current_t) * (sgl_size_t)num_operations));
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
        result = sgl_threadpool_attach_routine_consuming(
            pool,
            sgl_simd_resize_nearest_neighbor_routine,
            operations,
            (void *)data);
        sgl_queue_destroy(&operations);
    }
    SGL_SAFE_FREE(currents);
    SGL_SAFE_FREE(operations);

    return result;
}
#endif  /* !SGL_CFG_HAS_THREAD */

static sgl_result_t sgl_simd_resize_nearest_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_nearest_neighbor_data_t *data,
    sgl_int32_t d_height)
{
    sgl_result_t result;

    result = SGL_SUCCESS;
    if (pool == SGL_NULL) {
        sgl_simd_resize_nearest_single(data, d_height);
    }
#if defined(SGL_CFG_HAS_THREAD)
    else if (sgl_resize_nearest_should_use_threadpool(
                 pool, data->lut->d_width, d_height,
                 data->bpp) == SGL_FALSE) {
        sgl_simd_resize_nearest_single(data, d_height);
    }
    else {
        result = sgl_simd_resize_nearest_threaded(pool, data, d_height);
    }
#else
    else {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
#endif  /* !SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_simd_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_nearest_neighbor_data_t data;
    sgl_nearest_neighbor_lookup_t *lut = SGL_NULL;
    sgl_nearest_neighbor_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;

    SGL_TRACE_RESIZE_BEGIN(
        SGL_TRACE_BACKEND_SIMD,
        SGL_TRACE_METHOD_NEAREST,
        d_width,
        d_height,
        s_width,
        s_height,
        bpp,
        SGL_TRACE_REQUESTED_THREADS(pool),
        (ext_lut != SGL_NULL));
    errcnt = sgl_simd_resize_nearest_count_errors(
        dst, d_width, d_height, src, s_width, s_height, bpp);

    /* check error count */
    if (errcnt != 0) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }
    else if (sgl_simd_resize_nearest_is_same_size(
                 d_width, d_height, s_width, s_height) == SGL_TRUE) {
        sgl_simd_resize_nearest_copy_same_size(dst, d_width, d_height, src, bpp);
    }
    else {
        lut = sgl_simd_resize_nearest_select_lut(
            ext_lut, &temp_lut, d_width, d_height, s_width, s_height);
        if (lut != SGL_NULL) {
            sgl_simd_resize_nearest_set_data(
                &data, lut, dst, src, s_width, d_width, bpp);
            result = sgl_simd_resize_nearest_run(pool, &data, d_height);

            if (temp_lut != SGL_NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_nearest_neighbor_lut(temp_lut);
            }
        }
    }

    SGL_TRACE_RESIZE_END(
        SGL_TRACE_BACKEND_SIMD, SGL_TRACE_METHOD_NEAREST, result);

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_nearest_neighbor_current_t *cur = sgl_memory_as_const_nearest_neighbor_current(current);
    sgl_nearest_neighbor_data_t *data = sgl_memory_as_nearest_neighbor_data(cookie);
    sgl_int32_t row;

    if (sgl_simd_resize_nearest_uses_packed_range(data) == SGL_TRUE) {
        sgl_resize_nearest_neighbor_dispatch_packed_range(
            cur->row, cur->count, data);
    }
    else {
        for (row = cur->row; row < (cur->row + cur->count); ++row) {
            sgl_simd_resize_nearest_neighbor_line_stripe(row, data);
        }
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
