/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
/* SGL-CALLBACK-DEV-001: thread callbacks recover typed context from void *. */
/* cppcheck-suppress-file misra-c2012-11.5 */
/* cppcheck-suppress-file constParameterCallback */
#include <sgl-core.h>
#include "nearest_neighbor.h"

#define NEON_LANE_SIZE          (16)
#define NEON_HALF_LANE_SIZE     (8)

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_nearest_neighbor_upscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_half_lanes, sgl_int32_t half_step, sgl_int32_t bpp,
                                    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    sgl_int32_t col;
    sgl_int32_t lane;
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

    for (lane = 0; lane < num_half_lanes; ++lane) {
        col = (lane * NEON_HALF_LANE_SIZE);
        x_col_base = x[col];

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
            vtbl4_src = vld4q_u8(&src_y_buf[x[lane * NEON_HALF_LANE_SIZE] * SGL_BPP32]);
            value4.val[3] = vqtbl1_u8(vtbl4_src.val[3], vec_x_col);
            value4.val[2] = vqtbl1_u8(vtbl4_src.val[2], vec_x_col);
            value4.val[1] = vqtbl1_u8(vtbl4_src.val[1], vec_x_col);
            value4.val[0] = vqtbl1_u8(vtbl4_src.val[0], vec_x_col);
            vst4_u8(dst, value4);
            break;
        case SGL_BPP24:
            vtbl3_src = vld3q_u8(&src_y_buf[x[lane * NEON_HALF_LANE_SIZE] * SGL_BPP24]);
            value3.val[2] = vqtbl1_u8(vtbl3_src.val[2], vec_x_col);
            value3.val[1] = vqtbl1_u8(vtbl3_src.val[1], vec_x_col);
            value3.val[0] = vqtbl1_u8(vtbl3_src.val[0], vec_x_col);
            vst3_u8(dst, value3);
            break;
        case SGL_BPP16:
            vtbl2_src = vld2q_u8(&src_y_buf[x[lane * NEON_HALF_LANE_SIZE] * SGL_BPP16]);
            value2.val[1] = vqtbl1_u8(vtbl2_src.val[1], vec_x_col);
            value2.val[0] = vqtbl1_u8(vtbl2_src.val[0], vec_x_col);
            vst2_u8(dst, value2);
            break;
        case SGL_BPP8:
            vtbl1_src = vld1q_u8(&src_y_buf[x[lane * NEON_HALF_LANE_SIZE]]);
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

static SGL_ALWAYS_INLINE sgl_uint8_t *sgl_simd_resize_nearest_neighbor_downscale_line_stripe(
                                    sgl_int32_t row, sgl_int32_t num_lanes, sgl_int32_t step, sgl_int32_t bpp,
                                    sgl_nearest_neighbor_data_t *data)
{
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t lane;
    const sgl_int32_t *x;
    sgl_uint8_t *src_y_buf;
    sgl_uint8_t *dst;
    uint8x16x4_t value4;
    uint8x16x3_t value3;
    uint8x16x2_t value2;
    uint8x16_t value1;

    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

    for (lane = 0; lane < num_lanes; ++lane) {
        for (ch = 0; ch < SGL_BPP32; ++ch) {
            col = (lane * NEON_LANE_SIZE);
            switch (bpp) {
            case SGL_BPP32:
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 0);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 1);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 2);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 3);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 4);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 5);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 6);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 7);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 8);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 9);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 10);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 11);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 12);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 13);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 14);
                value4.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP32) + ch], value4.val[ch], 15);
                break;
            case SGL_BPP24:
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 0);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 1);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 2);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 3);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 4);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 5);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 6);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 7);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 8);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 9);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 10);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 11);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 12);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 13);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 14);
                value3.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP24) + ch], value3.val[ch], 15);
                break;
            case SGL_BPP16:
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 0);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 1);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 2);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 3);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 4);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 5);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 6);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 7);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 8);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 9);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 10);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 11);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 12);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 13);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 14);
                value2.val[ch] = vsetq_lane_u8(src_y_buf[(x[col++] * SGL_BPP16) + ch], value2.val[ch], 15);
                break;
            case SGL_BPP8:
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 0);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 1);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 2);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 3);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 4);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 5);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 6);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 7);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 8);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 9);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 10);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 11);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 12);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 13);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 14);
                value1 = vsetq_lane_u8(src_y_buf[x[col++]], value1, 15);
                break;
            default:
                /* Not Supported */
                break;
            }
        }

        switch (bpp) {
        case SGL_BPP32:
            vst4q_u8(dst, value4);
            break;
        case SGL_BPP24:
            vst3q_u8(dst, value3);
            break;
        case SGL_BPP16:
            vst2q_u8(dst, value2);
            break;
        case SGL_BPP8:
            vst1q_u8(dst, value1);
            break;
        default:
            /* Not Supported */
            break;
        }

        dst = &dst[step];
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
        num_lanes = d_width / NEON_HALF_LANE_SIZE;
        step = bpp * NEON_HALF_LANE_SIZE;
        lane_size = NEON_HALF_LANE_SIZE;
        dst = sgl_simd_resize_nearest_neighbor_upscale_line_stripe(row, num_lanes, step, bpp, data);
    }
    else {
        num_lanes = d_width / NEON_LANE_SIZE;
        step = bpp * NEON_LANE_SIZE;
        lane_size = NEON_LANE_SIZE;
        dst = sgl_simd_resize_nearest_neighbor_downscale_line_stripe(row, num_lanes, step, bpp, data);
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
            /* Not Supported */
            break;
        }
        for (ch = 0; ch < bpp; ++ch) {
            dst[ch] = src[ch];
        }
        dst = &dst[bpp];
    }
}

sgl_result_t sgl_simd_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_int32_t row;
    sgl_nearest_neighbor_data_t data;
    sgl_nearest_neighbor_lookup_t *lut = SGL_NULL;
    sgl_nearest_neighbor_lookup_t *temp_lut = SGL_NULL;
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

        if (((d_width != s_width) || (d_height != s_height)) && (lut == SGL_NULL)) {
            /* create temp look-up table */
            temp_lut = sgl_generic_create_nearest_neighbor_lut(d_width, d_height, s_width, s_height);
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
                    sgl_simd_resize_nearest_neighbor_line_stripe(row, (void *)&data);
                }
            }
#if defined(SGL_CFG_HAS_THREAD)
            else {
                sgl_nearest_neighbor_current_t *currents;
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
                currents = (sgl_nearest_neighbor_current_t *)sgl_malloc(sizeof(sgl_nearest_neighbor_current_t) * (sgl_size_t)num_operations);
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
                    (void)sgl_threadpool_attach_routine(pool, sgl_simd_resize_nearest_neighbor_routine, operations, (void *)&data);
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
                sgl_generic_destroy_nearest_neighbor_lut(temp_lut);
            }
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_simd_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_nearest_neighbor_current_t *cur = (const sgl_nearest_neighbor_current_t *)current;
    sgl_nearest_neighbor_data_t *data = (sgl_nearest_neighbor_data_t *)cookie;
    sgl_int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_simd_resize_nearest_neighbor_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
