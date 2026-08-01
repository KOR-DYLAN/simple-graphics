/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "neon_common.h"

#include "../threadpool/parallel_for.h"

#define SGL_NEON_CONVERT_CHANNEL_MAX          (255U)
#define SGL_NEON_CONVERT_MIN_PARALLEL_PIXELS  ((sgl_size_t)65536U)
#define SGL_NEON_CONVERT_INT32_MAX            ((sgl_size_t)2147483647U)
#define SGL_NEON_YUV_LANE_MASK                (7U)
#define SGL_NEON_YUV_MIN_PARALLEL_PIXELS      ((sgl_size_t)65536U)

/* Classify formats before selecting packed, planar, or subsampled kernels. */

static sgl_bool_t sgl_neon_is_rgb_packed(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_RGBA:
    case SGL_FOURCC_ARGB:
    case SGL_FOURCC_BGRA:
    case SGL_FOURCC_ABGR:
    case SGL_FOURCC_RGB:
    case SGL_FOURCC_BGR:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

static sgl_bool_t sgl_neon_is_rgb_planar(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_RGBA_PLANAR:
    case SGL_FOURCC_ARGB_PLANAR:
    case SGL_FOURCC_BGRA_PLANAR:
    case SGL_FOURCC_ABGR_PLANAR:
    case SGL_FOURCC_RGB_PLANAR:
    case SGL_FOURCC_BGR_PLANAR:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

sgl_bool_t sgl_neon_is_rgb(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    if ((sgl_neon_is_rgb_packed(format) == SGL_TRUE) ||
        (sgl_neon_is_rgb_planar(format) == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

sgl_bool_t sgl_neon_is_yuv444(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_YUV444_PACKED:
    case SGL_FOURCC_YUV444_PLANAR:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

sgl_result_t sgl_neon_validate(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((src == SGL_NULL) || (dst == SGL_NULL) ||
        (width <= 0) || (height <= 0)) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

/*
 * Convert dimensions to an addressable pixel count without overflowing
 * sgl_size_t. Callers validate positive dimensions before using the result.
 */
sgl_result_t sgl_neon_get_pixel_count(
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_size_t *pixels)
{
    sgl_uint64_t total;
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    total = (sgl_uint64_t)(sgl_uint32_t)width *
        (sgl_uint64_t)(sgl_uint32_t)height;
    if ((pixels != SGL_NULL) &&
        (total > 0U) &&
        (total <= (sgl_uint64_t)SGL_SIZE_MAX)) {
        *pixels = (sgl_size_t)total;
        result = SGL_SUCCESS;
    }

    return result;
}

/* Load eight packed pixels and normalize channel order to canonical RGBA. */
static sgl_neon_rgb_t sgl_neon_load_rgb_packed(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_size_t pixel)
{
    uint8x8x3_t channels3;
    uint8x8x4_t channels4;
    sgl_neon_rgb_t result;

    result.r = vdup_n_u8(0U);
    result.g = vdup_n_u8(0U);
    result.b = vdup_n_u8(0U);
    result.a = vdup_n_u8(SGL_NEON_CONVERT_CHANNEL_MAX);
    switch (format) {
    case SGL_FOURCC_RGBA:
        channels4 = vld4_u8(&src[pixel * 4U]);
        result.r = channels4.val[0];
        result.g = channels4.val[1];
        result.b = channels4.val[2];
        result.a = channels4.val[3];
        break;
    case SGL_FOURCC_ARGB:
        channels4 = vld4_u8(&src[pixel * 4U]);
        result.a = channels4.val[0];
        result.r = channels4.val[1];
        result.g = channels4.val[2];
        result.b = channels4.val[3];
        break;
    case SGL_FOURCC_BGRA:
        channels4 = vld4_u8(&src[pixel * 4U]);
        result.b = channels4.val[0];
        result.g = channels4.val[1];
        result.r = channels4.val[2];
        result.a = channels4.val[3];
        break;
    case SGL_FOURCC_ABGR:
        channels4 = vld4_u8(&src[pixel * 4U]);
        result.a = channels4.val[0];
        result.b = channels4.val[1];
        result.g = channels4.val[2];
        result.r = channels4.val[3];
        break;
    case SGL_FOURCC_BGR:
        channels3 = vld3_u8(&src[pixel * 3U]);
        result.b = channels3.val[0];
        result.g = channels3.val[1];
        result.r = channels3.val[2];
        break;
    default:
        channels3 = vld3_u8(&src[pixel * 3U]);
        result.r = channels3.val[0];
        result.g = channels3.val[1];
        result.b = channels3.val[2];
        break;
    }

    return result;
}

/* Load eight values from each channel plane into canonical RGBA vectors. */
static sgl_neon_rgb_t sgl_neon_load_rgb_planar(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel)
{
    sgl_neon_rgb_t result;

    result.r = vdup_n_u8(0U);
    result.g = vdup_n_u8(0U);
    result.b = vdup_n_u8(0U);
    result.a = vdup_n_u8(SGL_NEON_CONVERT_CHANNEL_MAX);
    switch (format) {
    case SGL_FOURCC_ARGB_PLANAR:
        result.a = vld1_u8(&src[pixel]);
        result.r = vld1_u8(&src[pixels + pixel]);
        result.g = vld1_u8(&src[(pixels * 2U) + pixel]);
        result.b = vld1_u8(&src[(pixels * 3U) + pixel]);
        break;
    case SGL_FOURCC_BGRA_PLANAR:
        result.b = vld1_u8(&src[pixel]);
        result.g = vld1_u8(&src[pixels + pixel]);
        result.r = vld1_u8(&src[(pixels * 2U) + pixel]);
        result.a = vld1_u8(&src[(pixels * 3U) + pixel]);
        break;
    case SGL_FOURCC_ABGR_PLANAR:
        result.a = vld1_u8(&src[pixel]);
        result.b = vld1_u8(&src[pixels + pixel]);
        result.g = vld1_u8(&src[(pixels * 2U) + pixel]);
        result.r = vld1_u8(&src[(pixels * 3U) + pixel]);
        break;
    case SGL_FOURCC_BGR_PLANAR:
        result.b = vld1_u8(&src[pixel]);
        result.g = vld1_u8(&src[pixels + pixel]);
        result.r = vld1_u8(&src[(pixels * 2U) + pixel]);
        break;
    case SGL_FOURCC_RGBA_PLANAR:
        result.r = vld1_u8(&src[pixel]);
        result.g = vld1_u8(&src[pixels + pixel]);
        result.b = vld1_u8(&src[(pixels * 2U) + pixel]);
        result.a = vld1_u8(&src[(pixels * 3U) + pixel]);
        break;
    default:
        result.r = vld1_u8(&src[pixel]);
        result.g = vld1_u8(&src[pixels + pixel]);
        result.b = vld1_u8(&src[(pixels * 2U) + pixel]);
        break;
    }

    return result;
}

sgl_neon_rgb_t sgl_neon_load_rgb(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel)
{
    sgl_neon_rgb_t result;

    if (sgl_neon_is_rgb_planar(format) == SGL_TRUE) {
        result = sgl_neon_load_rgb_planar(src, format, pixels, pixel);
    }
    else {
        result = sgl_neon_load_rgb_packed(src, format, pixel);
    }

    return result;
}

/* Store canonical RGBA vectors in the byte order selected by the FourCC. */
static void sgl_neon_store_rgb_packed(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_size_t pixel,
    sgl_neon_rgb_t value)
{
    uint8x8x3_t channels3;
    uint8x8x4_t channels4;

    switch (format) {
    case SGL_FOURCC_RGBA:
        channels4.val[0] = value.r;
        channels4.val[1] = value.g;
        channels4.val[2] = value.b;
        channels4.val[3] = value.a;
        vst4_u8(&dst[pixel * 4U], channels4);
        break;
    case SGL_FOURCC_ARGB:
        channels4.val[0] = value.a;
        channels4.val[1] = value.r;
        channels4.val[2] = value.g;
        channels4.val[3] = value.b;
        vst4_u8(&dst[pixel * 4U], channels4);
        break;
    case SGL_FOURCC_BGRA:
        channels4.val[0] = value.b;
        channels4.val[1] = value.g;
        channels4.val[2] = value.r;
        channels4.val[3] = value.a;
        vst4_u8(&dst[pixel * 4U], channels4);
        break;
    case SGL_FOURCC_ABGR:
        channels4.val[0] = value.a;
        channels4.val[1] = value.b;
        channels4.val[2] = value.g;
        channels4.val[3] = value.r;
        vst4_u8(&dst[pixel * 4U], channels4);
        break;
    case SGL_FOURCC_BGR:
        channels3.val[0] = value.b;
        channels3.val[1] = value.g;
        channels3.val[2] = value.r;
        vst3_u8(&dst[pixel * 3U], channels3);
        break;
    default:
        channels3.val[0] = value.r;
        channels3.val[1] = value.g;
        channels3.val[2] = value.b;
        vst3_u8(&dst[pixel * 3U], channels3);
        break;
    }
}

/* Store canonical RGBA vectors in independently contiguous channel planes. */
static void sgl_neon_store_rgb_planar(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel,
    sgl_neon_rgb_t value)
{
    switch (format) {
    case SGL_FOURCC_ARGB_PLANAR:
        vst1_u8(&dst[pixel], value.a);
        vst1_u8(&dst[pixels + pixel], value.r);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.g);
        vst1_u8(&dst[(pixels * 3U) + pixel], value.b);
        break;
    case SGL_FOURCC_BGRA_PLANAR:
        vst1_u8(&dst[pixel], value.b);
        vst1_u8(&dst[pixels + pixel], value.g);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.r);
        vst1_u8(&dst[(pixels * 3U) + pixel], value.a);
        break;
    case SGL_FOURCC_ABGR_PLANAR:
        vst1_u8(&dst[pixel], value.a);
        vst1_u8(&dst[pixels + pixel], value.b);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.g);
        vst1_u8(&dst[(pixels * 3U) + pixel], value.r);
        break;
    case SGL_FOURCC_BGR_PLANAR:
        vst1_u8(&dst[pixel], value.b);
        vst1_u8(&dst[pixels + pixel], value.g);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.r);
        break;
    case SGL_FOURCC_RGBA_PLANAR:
        vst1_u8(&dst[pixel], value.r);
        vst1_u8(&dst[pixels + pixel], value.g);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.b);
        vst1_u8(&dst[(pixels * 3U) + pixel], value.a);
        break;
    default:
        vst1_u8(&dst[pixel], value.r);
        vst1_u8(&dst[pixels + pixel], value.g);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.b);
        break;
    }
}

void sgl_neon_store_rgb(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel,
    sgl_neon_rgb_t value)
{
    if (sgl_neon_is_rgb_planar(format) == SGL_TRUE) {
        sgl_neon_store_rgb_planar(dst, format, pixels, pixel, value);
    }
    else {
        sgl_neon_store_rgb_packed(dst, format, pixel, value);
    }
}

/* YUV444 has one U and V sample per Y sample, so no expansion is required. */
sgl_neon_yuv_t sgl_neon_load_yuv444(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel)
{
    uint8x8x3_t channels;
    sgl_neon_yuv_t result;

    switch (format) {
    case SGL_FOURCC_YUV444_PLANAR:
        result.y = vld1_u8(&src[pixel]);
        result.u = vld1_u8(&src[pixels + pixel]);
        result.v = vld1_u8(&src[(pixels * 2U) + pixel]);
        break;
    default:
        channels = vld3_u8(&src[pixel * 3U]);
        result.y = channels.val[0];
        result.u = channels.val[1];
        result.v = channels.val[2];
        break;
    }

    return result;
}

void sgl_neon_store_yuv444(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel,
    sgl_neon_yuv_t value)
{
    uint8x8x3_t channels;

    switch (format) {
    case SGL_FOURCC_YUV444_PLANAR:
        vst1_u8(&dst[pixel], value.y);
        vst1_u8(&dst[pixels + pixel], value.u);
        vst1_u8(&dst[(pixels * 2U) + pixel], value.v);
        break;
    default:
        channels.val[0] = value.y;
        channels.val[1] = value.u;
        channels.val[2] = value.v;
        vst3_u8(&dst[pixel * 3U], channels);
        break;
    }
}

int16x8_t sgl_neon_widen_u8_to_s16(uint8x8_t value)
{
    int16x8_t result;

    result = vreinterpretq_s16_u16(vmovl_u8(value));

    return result;
}

/* Dispatch a contiguous pixel interval to a direction-specific SIMD kernel. */
static void sgl_neon_convert_run_range(
    const sgl_neon_convert_data_t *data,
    const sgl_parallel_range_t *range)
{
    data->kernel(
        data->src,
        data->src_format,
        data->dst,
        data->dst_format,
        data->pixels,
        (sgl_size_t)(sgl_uint32_t)range->start,
        (sgl_size_t)(sgl_uint32_t)range->count);
}

static void sgl_neon_convert_routine(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie)
{
    const sgl_parallel_range_t *range;
    const sgl_neon_convert_data_t *data;

    range = sgl_parallel_range_from_memory(current);
    /* SGL-MEM-DEV-001: typed conversion from threadpool cookie storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    data = (const sgl_neon_convert_data_t *)cookie;
    sgl_neon_convert_run_range(data, range);
}

static sgl_bool_t sgl_neon_convert_should_parallel(
    const sgl_threadpool_t *pool,
    sgl_size_t pixels)
{
    sgl_bool_t result = SGL_FALSE;

#if defined(SGL_CFG_HAS_THREAD)
    if ((pool != SGL_NULL) &&
        (sgl_threadpool_get_num_threads(pool) > 1U) &&
        (pixels >= SGL_NEON_CONVERT_MIN_PARALLEL_PIXELS) &&
        (pixels <= SGL_NEON_CONVERT_INT32_MAX)) {
        result = SGL_TRUE;
    }
#else
    (void)pool;
    (void)pixels;
#endif  /* SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_neon_convert_execute(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_neon_convert_data_t *data)
{
    sgl_result_t result = SGL_SUCCESS;

    if (sgl_neon_convert_should_parallel(
            pool, data->pixels) == SGL_TRUE) {
        result = sgl_parallel_for(
            pool,
            (sgl_int32_t)data->pixels,
            (sgl_int32_t)SGL_NEON_LANES,
            sgl_neon_convert_routine,
            (void *)data);
    }
    else {
        data->kernel(
            data->src,
            data->src_format,
            data->dst,
            data->dst_format,
            data->pixels,
            0U,
            data->pixels);
    }

    return result;
}

sgl_neon_convert_data_t sgl_neon_convert_make_data(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_size_t pixels,
    sgl_neon_convert_kernel_t kernel)
{
    sgl_neon_convert_data_t result;

    result.src = src;
    result.src_format = src_format;
    result.dst = dst;
    result.dst_format = dst_format;
    result.pixels = pixels;
    result.kernel = kernel;

    return result;
}

/* Classify YUV subsampling independently of packed or planar storage. */
static sgl_bool_t sgl_neon_is_yuv422_packed(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

sgl_bool_t sgl_neon_is_yuv422(sgl_fourcc_t format)
{
    sgl_bool_t result;

    result = sgl_neon_is_yuv422_packed(format);
    if (format == SGL_FOURCC_YUV422_PLANAR) {
        result = SGL_TRUE;
    }

    return result;
}

sgl_bool_t sgl_neon_is_yuv420(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_YUV420_PLANAR:
    case SGL_FOURCC_NV12:
    case SGL_FOURCC_NV21:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

/*
 * Expand four subsampled chroma bytes as [c0,c0,c1,c1,c2,c2,c3,c3].
 * Each duplicated lane then corresponds to one of the eight luma pixels.
 */
static uint8x8_t sgl_neon_yuv_duplicate_four(uint8x8_t value)
{
    uint8x8x2_t duplicated;

    duplicated = vzip_u8(value, value);

    return duplicated.val[0];
}

/* Load four chroma bytes without reading beyond the end of a chroma row. */
static uint8x8_t sgl_neon_yuv_load_four(
    const sgl_uint8_t *src,
    sgl_size_t offset)
{
    uint8x8_t value;

    value = vdup_n_u8(0U);
    value = vset_lane_u8(src[offset], value, 0);
    value = vset_lane_u8(src[offset + 1U], value, 1);
    value = vset_lane_u8(src[offset + 2U], value, 2);
    value = vset_lane_u8(src[offset + 3U], value, 3);
    value = sgl_neon_yuv_duplicate_four(value);

    return value;
}

/* Store the four even lanes produced by horizontal chroma downsampling. */
static void sgl_neon_yuv_store_four(
    sgl_uint8_t *dst,
    sgl_size_t offset,
    uint8x8_t value)
{
    dst[offset] = vget_lane_u8(value, 0);
    dst[offset + 1U] = vget_lane_u8(value, 1);
    dst[offset + 2U] = vget_lane_u8(value, 2);
    dst[offset + 3U] = vget_lane_u8(value, 3);
}

/* Rounded 2:1 horizontal average: (left + right + 1) >> 1. */
static uint8x8_t sgl_neon_yuv_average_two(uint8x8_t value)
{
    uint16x8_t sums;
    uint8x8_t result;

    sums = vpaddlq_u8(vcombine_u8(value, value));
    sums = vaddq_u16(sums, vdupq_n_u16(1U));
    sums = vshrq_n_u16(sums, 1);
    result = vmovn_u16(sums);

    return result;
}

/* Rounded 2x2 average: (top-left + top-right + bottom-left + bottom-right + 2) >> 2. */
static uint8x8_t sgl_neon_yuv_average_four(
    uint8x8_t top,
    uint8x8_t bottom)
{
    uint16x4_t sums;
    uint16x8_t duplicated;
    uint8x8_t result;

    sums = vadd_u16(vpaddl_u8(top), vpaddl_u8(bottom));
    sums = vadd_u16(sums, vdup_n_u16(2U));
    sums = vshr_n_u16(sums, 2);
    duplicated = vcombine_u16(sums, sums);
    result = vmovn_u16(duplicated);

    return result;
}

/* Decode eight luma pixels and duplicate their four packed 4:2:2 chroma samples. */
static sgl_neon_yuv_t sgl_neon_yuv_load_422_packed(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x)
{
    uint8x8x2_t channels;
    uint8x8x2_t chroma;
    uint8x8_t ordered;
    sgl_size_t offset;
    sgl_neon_yuv_t result;

    offset = ((((sgl_size_t)(sgl_uint32_t)row *
                ((sgl_size_t)(sgl_uint32_t)data->width >> 1U)) +
               ((sgl_size_t)(sgl_uint32_t)x >> 1U)) << 2U);
    channels = vld2_u8(&data->src[offset]);
    switch (data->src_format) {
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
        result.y = channels.val[1];
        ordered = channels.val[0];
        break;
    default:
        result.y = channels.val[0];
        ordered = channels.val[1];
        break;
    }
    chroma = vuzp_u8(ordered, ordered);
    switch (data->src_format) {
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_VYUY:
        result.v = sgl_neon_yuv_duplicate_four(chroma.val[0]);
        result.u = sgl_neon_yuv_duplicate_four(chroma.val[1]);
        break;
    default:
        result.u = sgl_neon_yuv_duplicate_four(chroma.val[0]);
        result.v = sgl_neon_yuv_duplicate_four(chroma.val[1]);
        break;
    }

    return result;
}

/* Load eight planar luma pixels and expand four planar 4:2:2 chroma samples. */
static sgl_neon_yuv_t sgl_neon_yuv_load_422_planar(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x)
{
    sgl_size_t pixel;
    sgl_size_t chroma_width;
    sgl_size_t chroma_size;
    sgl_size_t chroma_index;
    sgl_neon_yuv_t result;

    pixel = ((sgl_size_t)(sgl_uint32_t)row *
             (sgl_size_t)(sgl_uint32_t)data->width) +
        (sgl_size_t)(sgl_uint32_t)x;
    chroma_width = (sgl_size_t)(sgl_uint32_t)data->width >> 1U;
    chroma_size = chroma_width *
        (sgl_size_t)(sgl_uint32_t)data->height;
    chroma_index = ((sgl_size_t)(sgl_uint32_t)row * chroma_width) +
        ((sgl_size_t)(sgl_uint32_t)x >> 1U);
    result.y = vld1_u8(&data->src[pixel]);
    result.u = sgl_neon_yuv_load_four(
        data->src, data->pixels + chroma_index);
    result.v = sgl_neon_yuv_load_four(
        data->src, data->pixels + chroma_size + chroma_index);

    return result;
}

/*
 * Load eight luma pixels from one row and expand the four chroma samples shared
 * by that row pair. NV12 stores UV pairs; NV21 stores VU pairs.
 */
static sgl_neon_yuv_t sgl_neon_yuv_load_420(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x)
{
    uint8x8_t interleaved;
    uint8x8x2_t chroma;
    sgl_size_t pixel;
    sgl_size_t chroma_width;
    sgl_size_t chroma_height;
    sgl_size_t chroma_size;
    sgl_size_t chroma_index;
    sgl_neon_yuv_t result;

    pixel = ((sgl_size_t)(sgl_uint32_t)row *
             (sgl_size_t)(sgl_uint32_t)data->width) +
        (sgl_size_t)(sgl_uint32_t)x;
    chroma_width = (sgl_size_t)(sgl_uint32_t)data->width >> 1U;
    chroma_height = ((sgl_size_t)(sgl_uint32_t)data->height + 1U) >> 1U;
    chroma_size = chroma_width * chroma_height;
    chroma_index = (((sgl_size_t)(sgl_uint32_t)row >> 1U) *
                    chroma_width) +
        ((sgl_size_t)(sgl_uint32_t)x >> 1U);
    result.y = vld1_u8(&data->src[pixel]);
    switch (data->src_format) {
    case SGL_FOURCC_YUV420_PLANAR:
        result.u = sgl_neon_yuv_load_four(
            data->src, data->pixels + chroma_index);
        result.v = sgl_neon_yuv_load_four(
            data->src, data->pixels + chroma_size + chroma_index);
        break;
    default:
        interleaved = vld1_u8(
            &data->src[data->pixels + (chroma_index << 1U)]);
        chroma = vuzp_u8(interleaved, interleaved);
        if (data->src_format == SGL_FOURCC_NV21) {
            result.v = sgl_neon_yuv_duplicate_four(chroma.val[0]);
            result.u = sgl_neon_yuv_duplicate_four(chroma.val[1]);
        }
        else {
            result.u = sgl_neon_yuv_duplicate_four(chroma.val[0]);
            result.v = sgl_neon_yuv_duplicate_four(chroma.val[1]);
        }
        break;
    }

    return result;
}

sgl_neon_yuv_t sgl_neon_yuv_load(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x)
{
    sgl_size_t pixel;
    sgl_neon_yuv_t result;

    pixel = ((sgl_size_t)(sgl_uint32_t)row *
             (sgl_size_t)(sgl_uint32_t)data->width) +
        (sgl_size_t)(sgl_uint32_t)x;
    switch (data->src_format) {
    case SGL_FOURCC_YUV444_PACKED:
    case SGL_FOURCC_YUV444_PLANAR:
        result = sgl_neon_load_yuv444(
            data->src,
            data->src_format,
            data->pixels,
            pixel);
        break;
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
        result = sgl_neon_yuv_load_422_packed(data, row, x);
        break;
    case SGL_FOURCC_YUV422_PLANAR:
        result = sgl_neon_yuv_load_422_planar(data, row, x);
        break;
    default:
        result = sgl_neon_yuv_load_420(data, row, x);
        break;
    }

    return result;
}

/*
 * Write eight luma pixels and four rounded chroma averages. Packed formats
 * interleave two Y samples with one U/V pair; planar output stores 4-byte rows.
 */
void sgl_neon_yuv_store_422(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x,
    sgl_neon_yuv_t value)
{
    uint8x8x2_t output;
    uint8x8x2_t chroma;
    uint8x8_t u;
    uint8x8_t v;
    sgl_size_t pixel;
    sgl_size_t chroma_width;
    sgl_size_t chroma_size;
    sgl_size_t chroma_index;

    u = sgl_neon_yuv_average_two(value.u);
    v = sgl_neon_yuv_average_two(value.v);
    chroma_width = (sgl_size_t)(sgl_uint32_t)data->width >> 1U;
    chroma_index = ((sgl_size_t)(sgl_uint32_t)row * chroma_width) +
        ((sgl_size_t)(sgl_uint32_t)x >> 1U);
    if (data->dst_format == SGL_FOURCC_YUV422_PLANAR) {
        pixel = ((sgl_size_t)(sgl_uint32_t)row *
                 (sgl_size_t)(sgl_uint32_t)data->width) +
            (sgl_size_t)(sgl_uint32_t)x;
        chroma_size = chroma_width *
            (sgl_size_t)(sgl_uint32_t)data->height;
        vst1_u8(&data->dst[pixel], value.y);
        sgl_neon_yuv_store_four(
            data->dst, data->pixels + chroma_index, u);
        sgl_neon_yuv_store_four(
            data->dst, data->pixels + chroma_size + chroma_index, v);
    }
    else {
        switch (data->dst_format) {
        case SGL_FOURCC_YVYU:
        case SGL_FOURCC_VYUY:
            chroma = vzip_u8(v, u);
            break;
        default:
            chroma = vzip_u8(u, v);
            break;
        }
        switch (data->dst_format) {
        case SGL_FOURCC_UYVY:
        case SGL_FOURCC_VYUY:
            output.val[0] = chroma.val[0];
            output.val[1] = value.y;
            break;
        default:
            output.val[0] = value.y;
            output.val[1] = chroma.val[0];
            break;
        }
        vst2_u8(&data->dst[chroma_index << 2U], output);
    }
}

/*
 * Write two luma rows and four chroma samples averaged over each 2x2 block.
 * The caller owns an even row and supplies both rows to avoid shared writes.
 */
void sgl_neon_yuv_store_420(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x,
    sgl_neon_yuv_t top,
    sgl_neon_yuv_t bottom)
{
    uint8x8x2_t interleaved;
    uint8x8_t u;
    uint8x8_t v;
    sgl_size_t top_pixel;
    sgl_size_t bottom_pixel;
    sgl_size_t chroma_width;
    sgl_size_t chroma_height;
    sgl_size_t chroma_size;
    sgl_size_t chroma_index;

    top_pixel = ((sgl_size_t)(sgl_uint32_t)row *
                 (sgl_size_t)(sgl_uint32_t)data->width) +
        (sgl_size_t)(sgl_uint32_t)x;
    bottom_pixel = top_pixel +
        (sgl_size_t)(sgl_uint32_t)data->width;
    chroma_width = (sgl_size_t)(sgl_uint32_t)data->width >> 1U;
    chroma_height = (sgl_size_t)(sgl_uint32_t)data->height >> 1U;
    chroma_size = chroma_width * chroma_height;
    chroma_index = (((sgl_size_t)(sgl_uint32_t)row >> 1U) *
                    chroma_width) +
        ((sgl_size_t)(sgl_uint32_t)x >> 1U);
    u = sgl_neon_yuv_average_four(top.u, bottom.u);
    v = sgl_neon_yuv_average_four(top.v, bottom.v);
    vst1_u8(&data->dst[top_pixel], top.y);
    vst1_u8(&data->dst[bottom_pixel], bottom.y);
    switch (data->dst_format) {
    case SGL_FOURCC_YUV420_PLANAR:
        sgl_neon_yuv_store_four(
            data->dst, data->pixels + chroma_index, u);
        sgl_neon_yuv_store_four(
            data->dst, data->pixels + chroma_size + chroma_index, v);
        break;
    case SGL_FOURCC_NV21:
        interleaved = vzip_u8(v, u);
        vst1_u8(
            &data->dst[data->pixels + (chroma_index << 1U)],
            interleaved.val[0]);
        break;
    default:
        interleaved = vzip_u8(u, v);
        vst1_u8(
            &data->dst[data->pixels + (chroma_index << 1U)],
            interleaved.val[0]);
        break;
    }
}

/* Dispatch complete row ranges; row_alignment keeps 4:2:0 pairs together. */
static void sgl_neon_yuv_run_range(
    const sgl_neon_yuv_data_t *data,
    const sgl_parallel_range_t *range)
{
    data->kernel(data, range->start, range->count);
}

static void sgl_neon_yuv_routine(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie)
{
    const sgl_parallel_range_t *range;
    const sgl_neon_yuv_data_t *data;

    range = sgl_parallel_range_from_memory(current);
    /* SGL-MEM-DEV-001: typed conversion from threadpool cookie storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    data = (const sgl_neon_yuv_data_t *)cookie;
    sgl_neon_yuv_run_range(data, range);
}

static sgl_bool_t sgl_neon_yuv_should_parallel(
    const sgl_threadpool_t *pool,
    const sgl_neon_yuv_data_t *data)
{
    sgl_bool_t result = SGL_FALSE;

#if defined(SGL_CFG_HAS_THREAD)
    if ((pool != SGL_NULL) &&
        (sgl_threadpool_get_num_threads(pool) > 1U) &&
        (data->pixels >= SGL_NEON_YUV_MIN_PARALLEL_PIXELS)) {
        result = SGL_TRUE;
    }
#else
    (void)pool;
    (void)data;
#endif

    return result;
}

static sgl_result_t sgl_neon_yuv_execute(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_neon_yuv_data_t *data)
{
    sgl_result_t result = SGL_SUCCESS;

    if (sgl_neon_yuv_should_parallel(pool, data) == SGL_TRUE) {
        result = sgl_parallel_for(
            pool,
            data->height,
            data->row_alignment,
            sgl_neon_yuv_routine,
            (void *)data);
    }
    else {
        data->kernel(data, 0, data->height);
    }

    return result;
}

static sgl_bool_t sgl_neon_yuv_dimensions_supported(
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t row_alignment)
{
    sgl_bool_t result = SGL_FALSE;

    /* SIMD rows require eight pixels; 4:2:0 additionally requires row pairs. */
    if ((width >= (sgl_int32_t)SGL_NEON_LANES) &&
        (height > 0) &&
        (((sgl_uint32_t)width & SGL_NEON_YUV_LANE_MASK) == 0U) &&
        ((row_alignment == 1) ||
         (((sgl_uint32_t)height & 1U) == 0U))) {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_neon_yuv_data_t sgl_neon_yuv_make_data(
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t row_alignment,
    sgl_neon_yuv_kernel_t kernel)
{
    sgl_neon_yuv_data_t result;

    result.src = src;
    result.src_format = src_format;
    result.dst = dst;
    result.dst_format = dst_format;
    result.width = width;
    result.height = height;
    result.pixels = (sgl_size_t)(sgl_uint32_t)width *
        (sgl_size_t)(sgl_uint32_t)height;
    result.row_alignment = row_alignment;
    result.kernel = kernel;

    return result;
}

sgl_result_t sgl_neon_yuv_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t row_alignment,
    sgl_neon_yuv_kernel_t kernel)
{
    sgl_neon_yuv_data_t data;
    sgl_result_t result = SGL_ERROR_NOT_SUPPORTED;

    if (sgl_neon_yuv_dimensions_supported(
            width, height, row_alignment) == SGL_TRUE) {
        data = sgl_neon_yuv_make_data(
            src,
            src_format,
            dst,
            dst_format,
            width,
            height,
            row_alignment,
            kernel);
        result = sgl_neon_yuv_execute(pool, &data);
    }

    return result;
}
