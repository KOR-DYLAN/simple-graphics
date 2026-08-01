/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#include "generic_common.h"

#include <sgl-fixed_point.h>

#include "../threadpool/parallel_for.h"

/* Q8 conversion coefficients use eight fractional bits and 8-bit channels. */
#define SGL_CONVERT_CHANNEL_MAX    (255)
#define SGL_CONVERT_SCALE_BITS     (8U)
#if defined(SGL_CFG_HAS_THREAD)
#define SGL_CONVERT_MIN_PARALLEL_PIXELS    ((sgl_uint64_t)65536U)
#endif
#define SGL_CONVERT_PIXEL_INDEX(width, x, y) \
    (((sgl_size_t)(sgl_uint32_t)(y) * (sgl_size_t)(sgl_uint32_t)(width)) + \
     (sgl_size_t)(sgl_uint32_t)(x))
/* Add one then remove one low bit: ceil(dimension / 2). */
#define SGL_CONVERT_CHROMA_SIZE(dimension) \
    ((((sgl_size_t)(sgl_uint32_t)(dimension)) + (sgl_size_t)1U) >> 1U)
/* The uint64 form is used before checked conversion to sgl_size_t. */
#define SGL_CONVERT_CHROMA_U64(dimension) \
    ((((sgl_uint64_t)(sgl_uint32_t)(dimension)) + (sgl_uint64_t)1U) >> 1U)
/* Remove one low coordinate bit to select a shared 2-pixel sample. */
#define SGL_CONVERT_HALF_SIZE(value) \
    (((sgl_size_t)(sgl_uint32_t)(value)) >> 1U)

static sgl_uint64_t sgl_convert_yuv_buffer_size_u64(
    sgl_fourcc_t format,
    sgl_uint64_t pixels,
    sgl_uint64_t chroma_width,
    sgl_uint64_t chroma_height,
    sgl_int32_t height);

/* Classify layouts before any format-specific offset calculation. */
static sgl_bool_t sgl_convert_is_rgb_packed(sgl_fourcc_t format)
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

static sgl_bool_t sgl_convert_is_rgb_planar(sgl_fourcc_t format)
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

sgl_bool_t sgl_convert_is_rgb_format(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    if ((sgl_convert_is_rgb_packed(format) == SGL_TRUE) ||
        (sgl_convert_is_rgb_planar(format) == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

static sgl_bool_t sgl_convert_rgb_has_alpha(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_RGBA:
    case SGL_FOURCC_ARGB:
    case SGL_FOURCC_BGRA:
    case SGL_FOURCC_ABGR:
    case SGL_FOURCC_RGBA_PLANAR:
    case SGL_FOURCC_ARGB_PLANAR:
    case SGL_FOURCC_BGRA_PLANAR:
    case SGL_FOURCC_ABGR_PLANAR:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

static sgl_bool_t sgl_convert_is_yuv_444(sgl_fourcc_t format)
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

static sgl_bool_t sgl_convert_is_yuv_422_packed(sgl_fourcc_t format)
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

static sgl_bool_t sgl_convert_is_yuv_422_planar(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    switch (format) {
    case SGL_FOURCC_YUV422_PLANAR:
        result = SGL_TRUE;
        break;
    default:
        result = SGL_FALSE;
        break;
    }

    return result;
}

static sgl_bool_t sgl_convert_is_yuv_420(sgl_fourcc_t format)
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

sgl_bool_t sgl_convert_is_yuv_format(sgl_fourcc_t format)
{
    sgl_bool_t result = SGL_FALSE;

    if ((sgl_convert_is_yuv_444(format) == SGL_TRUE) ||
        (sgl_convert_is_yuv_422_packed(format) == SGL_TRUE) ||
        (sgl_convert_is_yuv_422_planar(format) == SGL_TRUE) ||
        (sgl_convert_is_yuv_420(format) == SGL_TRUE)) {
        result = SGL_TRUE;
    }

    return result;
}

sgl_result_t sgl_convert_validate_common(
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

static sgl_int32_t sgl_convert_min_int32(sgl_int32_t lhs, sgl_int32_t rhs)
{
    sgl_int32_t result = lhs;

    if (rhs < lhs) {
        result = rhs;
    }

    return result;
}

/* Shared scalar arithmetic for the forward and inverse Q8 color matrices. */
sgl_uint8_t sgl_convert_clip_int32(sgl_int32_t value)
{
    sgl_uint8_t result;

    if (value < 0) {
        result = 0U;
    }
    else if (value > SGL_CONVERT_CHANNEL_MAX) {
        result = (sgl_uint8_t)SGL_CONVERT_CHANNEL_MAX;
    }
    else {
        result = (sgl_uint8_t)value;
    }

    return result;
}

sgl_int32_t sgl_convert_divide_256(sgl_int32_t value)
{
    sgl_int32_t result;

    /*
     * Remove eight coefficient fractional bits. The helper preserves C99
     * truncation toward zero for negative chroma terms.
     */
    result = (sgl_int32_t)sgl_fixed_div_pow2_i64(
        (sgl_int64_t)value, SGL_CONVERT_SCALE_BITS);

    return result;
}

sgl_uint8_t sgl_convert_average_chroma(
    sgl_int32_t sum,
    sgl_int32_t count)
{
    sgl_uint8_t result;

    switch (count) {
    case 1:
        result = (sgl_uint8_t)sum;
        break;
    case 2:
        /* Add half the divisor, then remove one bit: rounded sum / 2. */
        result = (sgl_uint8_t)(
            ((sgl_uint32_t)sum + 1U) >> 1U);
        break;
    case 4:
        /* Add half the divisor, then remove two bits: rounded sum / 4. */
        result = (sgl_uint8_t)(
            ((sgl_uint32_t)sum + 2U) >> 2U);
        break;
    default:
        result = 0U;
        break;
    }

    return result;
}

sgl_size_t sgl_convert_buffer_size(
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_uint64_t pixels;
    sgl_uint64_t chroma_width;
    sgl_uint64_t chroma_height;
    sgl_uint64_t total = 0U;
    sgl_size_t result = 0U;

    pixels = (sgl_uint64_t)(sgl_uint32_t)width *
        (sgl_uint64_t)(sgl_uint32_t)height;
    chroma_width = SGL_CONVERT_CHROMA_U64(width);
    chroma_height = SGL_CONVERT_CHROMA_U64(height);

    /*
     * RGB layouts always contain three or four full-size planes/channels.
     * YUV layouts derive their size from horizontal and vertical chroma
     * subsampling, including rounded-up chroma dimensions for odd images.
     */
    if (sgl_convert_is_rgb_format(format) == SGL_TRUE) {
        if (sgl_convert_rgb_has_alpha(format) == SGL_TRUE) {
            total = pixels * 4U;
        }
        else {
            total = pixels * 3U;
        }
    }
    else {
        total = sgl_convert_yuv_buffer_size_u64(
            format, pixels, chroma_width, chroma_height, height);
    }

    if ((total > 0U) && (total <= (sgl_uint64_t)SGL_SIZE_MAX)) {
        result = (sgl_size_t)total;
    }

    return result;
}

static sgl_uint64_t sgl_convert_yuv_buffer_size_u64(
    sgl_fourcc_t format,
    sgl_uint64_t pixels,
    sgl_uint64_t chroma_width,
    sgl_uint64_t chroma_height,
    sgl_int32_t height)
{
    sgl_uint64_t result;

    switch (format) {
    case SGL_FOURCC_YUV444_PACKED:
    case SGL_FOURCC_YUV444_PLANAR:
        result = pixels * 3U;
        break;
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
        result = chroma_width *
            (sgl_uint64_t)(sgl_uint32_t)height * 4U;
        break;
    case SGL_FOURCC_YUV422_PLANAR:
        result = pixels +
            (2U * chroma_width *
             (sgl_uint64_t)(sgl_uint32_t)height);
        break;
    default:
        result = pixels + (2U * chroma_width * chroma_height);
        break;
    }

    return result;
}

sgl_result_t sgl_convert_validate_buffer_sizes(
    sgl_fourcc_t src_format,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_result_t result = SGL_SUCCESS;

    if ((sgl_convert_buffer_size(src_format, width, height) == 0U) ||
        (sgl_convert_buffer_size(dst_format, width, height) == 0U)) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

/* Normalize every RGB byte order into sgl_convert_rgb_t. */
static sgl_convert_rgb_t sgl_convert_read_rgb_packed(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_rgb_t result;
    sgl_size_t offset;
    sgl_int32_t bpp = SGL_BPP32;

    switch (format) {
    case SGL_FOURCC_RGB:
    case SGL_FOURCC_BGR:
        bpp = SGL_BPP24;
        break;
    default:
        bpp = SGL_BPP32;
        break;
    }
    offset = SGL_CONVERT_PIXEL_INDEX(width, x, y) * (sgl_size_t)(sgl_uint32_t)bpp;
    result.r = 0U;
    result.g = 0U;
    result.b = 0U;
    result.a = (sgl_uint8_t)SGL_CONVERT_CHANNEL_MAX;

    switch (format) {
    case SGL_FOURCC_RGBA:
        result.r = src[offset];
        result.g = src[offset + 1U];
        result.b = src[offset + 2U];
        result.a = src[offset + 3U];
        break;
    case SGL_FOURCC_ARGB:
        result.a = src[offset];
        result.r = src[offset + 1U];
        result.g = src[offset + 2U];
        result.b = src[offset + 3U];
        break;
    case SGL_FOURCC_BGRA:
        result.b = src[offset];
        result.g = src[offset + 1U];
        result.r = src[offset + 2U];
        result.a = src[offset + 3U];
        break;
    case SGL_FOURCC_ABGR:
        result.a = src[offset];
        result.b = src[offset + 1U];
        result.g = src[offset + 2U];
        result.r = src[offset + 3U];
        break;
    case SGL_FOURCC_BGR:
        result.b = src[offset];
        result.g = src[offset + 1U];
        result.r = src[offset + 2U];
        break;
    default:
        result.r = src[offset];
        result.g = src[offset + 1U];
        result.b = src[offset + 2U];
        break;
    }

    return result;
}

static sgl_convert_rgb_t sgl_convert_read_rgb_planar(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_rgb_t result;
    sgl_size_t pixel;
    sgl_size_t plane_size;

    pixel = SGL_CONVERT_PIXEL_INDEX(width, x, y);
    plane_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
    result.r = 0U;
    result.g = 0U;
    result.b = 0U;
    result.a = (sgl_uint8_t)SGL_CONVERT_CHANNEL_MAX;

    switch (format) {
    case SGL_FOURCC_ARGB_PLANAR:
        result.a = src[pixel];
        result.r = src[plane_size + pixel];
        result.g = src[(plane_size * 2U) + pixel];
        result.b = src[(plane_size * 3U) + pixel];
        break;
    case SGL_FOURCC_BGRA_PLANAR:
        result.b = src[pixel];
        result.g = src[plane_size + pixel];
        result.r = src[(plane_size * 2U) + pixel];
        result.a = src[(plane_size * 3U) + pixel];
        break;
    case SGL_FOURCC_ABGR_PLANAR:
        result.a = src[pixel];
        result.b = src[plane_size + pixel];
        result.g = src[(plane_size * 2U) + pixel];
        result.r = src[(plane_size * 3U) + pixel];
        break;
    case SGL_FOURCC_BGR_PLANAR:
        result.b = src[pixel];
        result.g = src[plane_size + pixel];
        result.r = src[(plane_size * 2U) + pixel];
        break;
    case SGL_FOURCC_RGBA_PLANAR:
        result.r = src[pixel];
        result.g = src[plane_size + pixel];
        result.b = src[(plane_size * 2U) + pixel];
        result.a = src[(plane_size * 3U) + pixel];
        break;
    default:
        result.r = src[pixel];
        result.g = src[plane_size + pixel];
        result.b = src[(plane_size * 2U) + pixel];
        break;
    }

    return result;
}

sgl_convert_rgb_t sgl_convert_read_rgb(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_rgb_t result;

    if (sgl_convert_is_rgb_planar(format) == SGL_TRUE) {
        result = sgl_convert_read_rgb_planar(src, format, width, height, x, y);
    }
    else {
        result = sgl_convert_read_rgb_packed(src, format, width, x, y);
    }

    return result;
}

/* Serialize the canonical RGB sample into the requested byte or plane order. */
static void sgl_convert_write_rgb_packed(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_rgb_t rgb)
{
    sgl_size_t offset;
    sgl_int32_t bpp = SGL_BPP32;

    switch (format) {
    case SGL_FOURCC_RGB:
    case SGL_FOURCC_BGR:
        bpp = SGL_BPP24;
        break;
    default:
        bpp = SGL_BPP32;
        break;
    }
    offset = SGL_CONVERT_PIXEL_INDEX(width, x, y) * (sgl_size_t)(sgl_uint32_t)bpp;

    switch (format) {
    case SGL_FOURCC_RGBA:
        dst[offset] = rgb.r;
        dst[offset + 1U] = rgb.g;
        dst[offset + 2U] = rgb.b;
        dst[offset + 3U] = rgb.a;
        break;
    case SGL_FOURCC_ARGB:
        dst[offset] = rgb.a;
        dst[offset + 1U] = rgb.r;
        dst[offset + 2U] = rgb.g;
        dst[offset + 3U] = rgb.b;
        break;
    case SGL_FOURCC_BGRA:
        dst[offset] = rgb.b;
        dst[offset + 1U] = rgb.g;
        dst[offset + 2U] = rgb.r;
        dst[offset + 3U] = rgb.a;
        break;
    case SGL_FOURCC_ABGR:
        dst[offset] = rgb.a;
        dst[offset + 1U] = rgb.b;
        dst[offset + 2U] = rgb.g;
        dst[offset + 3U] = rgb.r;
        break;
    case SGL_FOURCC_BGR:
        dst[offset] = rgb.b;
        dst[offset + 1U] = rgb.g;
        dst[offset + 2U] = rgb.r;
        break;
    default:
        dst[offset] = rgb.r;
        dst[offset + 1U] = rgb.g;
        dst[offset + 2U] = rgb.b;
        break;
    }
}

static void sgl_convert_write_rgb_planar(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_rgb_t rgb)
{
    sgl_size_t pixel;
    sgl_size_t plane_size;

    pixel = SGL_CONVERT_PIXEL_INDEX(width, x, y);
    plane_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;

    switch (format) {
    case SGL_FOURCC_ARGB_PLANAR:
        dst[pixel] = rgb.a;
        dst[plane_size + pixel] = rgb.r;
        dst[(plane_size * 2U) + pixel] = rgb.g;
        dst[(plane_size * 3U) + pixel] = rgb.b;
        break;
    case SGL_FOURCC_BGRA_PLANAR:
        dst[pixel] = rgb.b;
        dst[plane_size + pixel] = rgb.g;
        dst[(plane_size * 2U) + pixel] = rgb.r;
        dst[(plane_size * 3U) + pixel] = rgb.a;
        break;
    case SGL_FOURCC_ABGR_PLANAR:
        dst[pixel] = rgb.a;
        dst[plane_size + pixel] = rgb.b;
        dst[(plane_size * 2U) + pixel] = rgb.g;
        dst[(plane_size * 3U) + pixel] = rgb.r;
        break;
    case SGL_FOURCC_BGR_PLANAR:
        dst[pixel] = rgb.b;
        dst[plane_size + pixel] = rgb.g;
        dst[(plane_size * 2U) + pixel] = rgb.r;
        break;
    case SGL_FOURCC_RGBA_PLANAR:
        dst[pixel] = rgb.r;
        dst[plane_size + pixel] = rgb.g;
        dst[(plane_size * 2U) + pixel] = rgb.b;
        dst[(plane_size * 3U) + pixel] = rgb.a;
        break;
    default:
        dst[pixel] = rgb.r;
        dst[plane_size + pixel] = rgb.g;
        dst[(plane_size * 2U) + pixel] = rgb.b;
        break;
    }
}

void sgl_convert_write_rgb(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_rgb_t rgb)
{
    if (sgl_convert_is_rgb_planar(format) == SGL_TRUE) {
        sgl_convert_write_rgb_planar(dst, format, width, height, x, y, rgb);
    }
    else {
        sgl_convert_write_rgb_packed(dst, format, width, x, y, rgb);
    }
}

/* Read YUV while expanding shared 4:2:2 or 4:2:0 chroma to one pixel sample. */
static sgl_convert_yuv_t sgl_convert_read_yuv_444(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_yuv_t result;
    sgl_size_t pixel;
    sgl_size_t offset;
    sgl_size_t plane_size;

    pixel = SGL_CONVERT_PIXEL_INDEX(width, x, y);
    switch (format) {
    case SGL_FOURCC_YUV444_PLANAR:
        plane_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
        result.y = src[pixel];
        result.u = src[plane_size + pixel];
        result.v = src[(plane_size * 2U) + pixel];
        break;
    default:
        offset = pixel * 3U;
        result.y = src[offset];
        result.u = src[offset + 1U];
        result.v = src[offset + 2U];
        break;
    }

    return result;
}

static sgl_convert_yuv_t sgl_convert_read_yuv_422_packed(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_yuv_t result;
    sgl_size_t offset;
    sgl_int32_t even_pixel;
    sgl_uint32_t parity;

    offset = (((sgl_size_t)(sgl_uint32_t)y *
               SGL_CONVERT_CHROMA_SIZE(width)) +
              SGL_CONVERT_HALF_SIZE(x)) * 4U;
    /* The low x bit is zero for the first luma and one for the second. */
    parity = (sgl_uint32_t)x & 1U;
    parity <<= 1U;
    even_pixel = (sgl_int32_t)parity;

    switch (format) {
    case SGL_FOURCC_YVYU:
        result.y = src[offset + (sgl_size_t)(sgl_uint32_t)even_pixel];
        result.u = src[offset + 3U];
        result.v = src[offset + 1U];
        break;
    case SGL_FOURCC_UYVY:
        result.y = src[
            offset + 1U + (sgl_size_t)(sgl_uint32_t)even_pixel];
        result.u = src[offset];
        result.v = src[offset + 2U];
        break;
    case SGL_FOURCC_VYUY:
        result.y = src[
            offset + 1U + (sgl_size_t)(sgl_uint32_t)even_pixel];
        result.u = src[offset + 2U];
        result.v = src[offset];
        break;
    default:
        result.y = src[offset + (sgl_size_t)(sgl_uint32_t)even_pixel];
        result.u = src[offset + 1U];
        result.v = src[offset + 3U];
        break;
    }

    return result;
}

static sgl_convert_yuv_t sgl_convert_read_yuv_422_planar(
    const sgl_uint8_t *src,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_yuv_t result;
    sgl_size_t y_size;
    sgl_size_t chroma_index;
    sgl_size_t chroma_size;

    y_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
    chroma_size = SGL_CONVERT_CHROMA_SIZE(width) *
                  (sgl_size_t)(sgl_uint32_t)height;
    chroma_index = ((sgl_size_t)(sgl_uint32_t)y *
                    SGL_CONVERT_CHROMA_SIZE(width)) +
                   SGL_CONVERT_HALF_SIZE(x);

    result.y = src[SGL_CONVERT_PIXEL_INDEX(width, x, y)];
    result.u = src[y_size + chroma_index];
    result.v = src[y_size + chroma_size + chroma_index];

    return result;
}

static sgl_convert_yuv_t sgl_convert_read_yuv_420(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_yuv_t result;
    sgl_size_t y_size;
    sgl_size_t chroma_index;
    sgl_size_t chroma_size;
    sgl_size_t offset;

    y_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
    chroma_index = (SGL_CONVERT_HALF_SIZE(y) *
                    SGL_CONVERT_CHROMA_SIZE(width)) +
                   SGL_CONVERT_HALF_SIZE(x);
    result.y = src[SGL_CONVERT_PIXEL_INDEX(width, x, y)];

    switch (format) {
    case SGL_FOURCC_YUV420_PLANAR:
        chroma_size = SGL_CONVERT_CHROMA_SIZE(width) *
                      SGL_CONVERT_CHROMA_SIZE(height);
        result.u = src[y_size + chroma_index];
        result.v = src[y_size + chroma_size + chroma_index];
        break;
    case SGL_FOURCC_NV21:
        offset = y_size + (chroma_index * 2U);
        result.v = src[offset];
        result.u = src[offset + 1U];
        break;
    default:
        offset = y_size + (chroma_index * 2U);
        result.u = src[offset];
        result.v = src[offset + 1U];
        break;
    }

    return result;
}

sgl_convert_yuv_t sgl_convert_read_yuv(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_yuv_t result;

    switch (format) {
    case SGL_FOURCC_YUV444_PACKED:
    case SGL_FOURCC_YUV444_PLANAR:
        result = sgl_convert_read_yuv_444(src, format, width, height, x, y);
        break;
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
        result = sgl_convert_read_yuv_422_packed(src, format, width, x, y);
        break;
    case SGL_FOURCC_YUV422_PLANAR:
        result = sgl_convert_read_yuv_422_planar(src, width, height, x, y);
        break;
    default:
        result = sgl_convert_read_yuv_420(src, format, width, height, x, y);
        break;
    }

    return result;
}

/* Luma is always written per pixel, even when chroma is subsampled. */
static void sgl_convert_write_yuv_444_luma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_uint8_t value)
{
    sgl_size_t offset;

    switch (format) {
    case SGL_FOURCC_YUV444_PLANAR:
        dst[SGL_CONVERT_PIXEL_INDEX(width, x, y)] = value;
        break;
    default:
        offset = SGL_CONVERT_PIXEL_INDEX(width, x, y) * 3U;
        dst[offset] = value;
        SGL_UNUSED_PARAM(height);
        break;
    }
}

static void sgl_convert_write_yuv_422_luma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_uint8_t value)
{
    sgl_size_t offset;
    sgl_size_t y_offset;
    sgl_uint32_t parity;

    switch (format) {
    case SGL_FOURCC_YUV422_PLANAR:
        dst[SGL_CONVERT_PIXEL_INDEX(width, x, y)] = value;
        break;
    default:
        offset = (((sgl_size_t)(sgl_uint32_t)y *
                   SGL_CONVERT_CHROMA_SIZE(width)) +
                  SGL_CONVERT_HALF_SIZE(x)) * 4U;
        /* Multiplying the low x bit by two selects Y0 (0) or Y1 (2). */
        parity = (sgl_uint32_t)x & 1U;
        y_offset = (sgl_size_t)parity;
        y_offset <<= 1U;
        switch (format) {
        case SGL_FOURCC_UYVY:
        case SGL_FOURCC_VYUY:
            y_offset += 1U;
            break;
        default:
            break;
        }
        dst[offset + y_offset] = value;
        if (parity == 0U) {
            /*
             * Initialize the paired luma slot as well. A real odd pixel
             * overwrites it; an odd-width row retains a defined padding Y.
             */
            dst[offset + y_offset + 2U] = value;
        }
        break;
    }
}

static void sgl_convert_write_yuv_420_luma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_uint8_t value)
{
    dst[SGL_CONVERT_PIXEL_INDEX(width, x, y)] = value;
    SGL_UNUSED_PARAM(format);
}

void sgl_convert_write_yuv_luma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_uint8_t value)
{
    switch (format) {
    case SGL_FOURCC_YUV444_PACKED:
    case SGL_FOURCC_YUV444_PLANAR:
        sgl_convert_write_yuv_444_luma(dst, format, width, height, x, y, value);
        break;
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
    case SGL_FOURCC_YUV422_PLANAR:
        sgl_convert_write_yuv_422_luma(dst, format, width, x, y, value);
        SGL_UNUSED_PARAM(height);
        break;
    default:
        sgl_convert_write_yuv_420_luma(dst, format, width, x, y, value);
        SGL_UNUSED_PARAM(height);
        break;
    }
}

/* Chroma writers store one averaged sample for each destination group. */
static void sgl_convert_write_yuv_444_chroma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_yuv_t value)
{
    sgl_size_t pixel;
    sgl_size_t offset;
    sgl_size_t plane_size;

    pixel = SGL_CONVERT_PIXEL_INDEX(width, x, y);
    switch (format) {
    case SGL_FOURCC_YUV444_PLANAR:
        plane_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
        dst[plane_size + pixel] = value.u;
        dst[(plane_size * 2U) + pixel] = value.v;
        break;
    default:
        offset = pixel * 3U;
        dst[offset + 1U] = value.u;
        dst[offset + 2U] = value.v;
        SGL_UNUSED_PARAM(height);
        break;
    }
}

static void sgl_convert_write_yuv_422_chroma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_yuv_t value)
{
    sgl_size_t offset;
    sgl_size_t y_size;
    sgl_size_t chroma_index;
    sgl_size_t chroma_size;

    switch (format) {
    case SGL_FOURCC_YUV422_PLANAR:
        y_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
        chroma_size = SGL_CONVERT_CHROMA_SIZE(width) *
                      (sgl_size_t)(sgl_uint32_t)height;
        chroma_index = ((sgl_size_t)(sgl_uint32_t)y *
                        SGL_CONVERT_CHROMA_SIZE(width)) +
                       SGL_CONVERT_HALF_SIZE(x);
        dst[y_size + chroma_index] = value.u;
        dst[y_size + chroma_size + chroma_index] = value.v;
        break;
    case SGL_FOURCC_YVYU:
        offset = (((sgl_size_t)(sgl_uint32_t)y *
                   SGL_CONVERT_CHROMA_SIZE(width)) +
                  SGL_CONVERT_HALF_SIZE(x)) * 4U;
        dst[offset + 1U] = value.v;
        dst[offset + 3U] = value.u;
        break;
    case SGL_FOURCC_UYVY:
        offset = (((sgl_size_t)(sgl_uint32_t)y *
                   SGL_CONVERT_CHROMA_SIZE(width)) +
                  SGL_CONVERT_HALF_SIZE(x)) * 4U;
        dst[offset] = value.u;
        dst[offset + 2U] = value.v;
        break;
    case SGL_FOURCC_VYUY:
        offset = (((sgl_size_t)(sgl_uint32_t)y *
                   SGL_CONVERT_CHROMA_SIZE(width)) +
                  SGL_CONVERT_HALF_SIZE(x)) * 4U;
        dst[offset] = value.v;
        dst[offset + 2U] = value.u;
        break;
    default:
        offset = (((sgl_size_t)(sgl_uint32_t)y *
                   SGL_CONVERT_CHROMA_SIZE(width)) +
                  SGL_CONVERT_HALF_SIZE(x)) * 4U;
        dst[offset + 1U] = value.u;
        dst[offset + 3U] = value.v;
        break;
    }
}

static void sgl_convert_write_yuv_420_chroma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_yuv_t value)
{
    sgl_size_t y_size;
    sgl_size_t chroma_index;
    sgl_size_t chroma_size;
    sgl_size_t offset;

    y_size = (sgl_size_t)(sgl_uint32_t)width * (sgl_size_t)(sgl_uint32_t)height;
    chroma_index = (SGL_CONVERT_HALF_SIZE(y) *
                    SGL_CONVERT_CHROMA_SIZE(width)) +
                   SGL_CONVERT_HALF_SIZE(x);

    switch (format) {
    case SGL_FOURCC_YUV420_PLANAR:
        chroma_size = SGL_CONVERT_CHROMA_SIZE(width) *
                      SGL_CONVERT_CHROMA_SIZE(height);
        dst[y_size + chroma_index] = value.u;
        dst[y_size + chroma_size + chroma_index] = value.v;
        break;
    case SGL_FOURCC_NV21:
        offset = y_size + (chroma_index * 2U);
        dst[offset] = value.v;
        dst[offset + 1U] = value.u;
        break;
    default:
        offset = y_size + (chroma_index * 2U);
        dst[offset] = value.u;
        dst[offset + 1U] = value.v;
        break;
    }
}

void sgl_convert_write_yuv_chroma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_yuv_t value)
{
    switch (format) {
    case SGL_FOURCC_YUV444_PACKED:
    case SGL_FOURCC_YUV444_PLANAR:
        sgl_convert_write_yuv_444_chroma(dst, format, width, height, x, y, value);
        break;
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
    case SGL_FOURCC_YUV422_PLANAR:
        sgl_convert_write_yuv_422_chroma(dst, format, width, height, x, y, value);
        break;
    default:
        sgl_convert_write_yuv_420_chroma(dst, format, width, height, x, y, value);
        break;
    }
}

/* Destination group dimensions define chroma averaging and row alignment. */
sgl_int32_t sgl_convert_yuv_group_width(sgl_fourcc_t format)
{
    sgl_int32_t result = 1;

    switch (format) {
    case SGL_FOURCC_YUYV:
    case SGL_FOURCC_YVYU:
    case SGL_FOURCC_UYVY:
    case SGL_FOURCC_VYUY:
    case SGL_FOURCC_YUV422_PLANAR:
    case SGL_FOURCC_YUV420_PLANAR:
    case SGL_FOURCC_NV12:
    case SGL_FOURCC_NV21:
        result = 2;
        break;
    default:
        result = 1;
        break;
    }

    return result;
}

sgl_int32_t sgl_convert_yuv_group_height(sgl_fourcc_t format)
{
    sgl_int32_t result = 1;

    switch (format) {
    case SGL_FOURCC_YUV420_PLANAR:
    case SGL_FOURCC_NV12:
    case SGL_FOURCC_NV21:
        result = 2;
        break;
    default:
        result = 1;
        break;
    }

    return result;
}

sgl_convert_block_t sgl_convert_make_block(
    const sgl_convert_request_t *request,
    sgl_int32_t x,
    sgl_int32_t y)
{
    sgl_convert_block_t result;
    sgl_int32_t group_width;
    sgl_int32_t group_height;

    group_width = sgl_convert_yuv_group_width(request->dst_format);
    group_height = sgl_convert_yuv_group_height(request->dst_format);

    result.x = x;
    result.y = y;
    result.width = sgl_convert_min_int32(group_width, request->width - x);
    result.height = sgl_convert_min_int32(group_height, request->height - y);

    return result;
}

/* Dispatch row-aligned ranges without exposing threadpool details to kernels. */
static void sgl_convert_run_range(
    sgl_convert_request_t *request,
    const sgl_parallel_range_t *range)
{
    request->kernel(request, range->start, range->count);
}

/* cppcheck-suppress constParameterCallback */
static void sgl_convert_routine(
    void *SGL_RESTRICT current,
    void *SGL_RESTRICT cookie)
{
    const sgl_parallel_range_t *range;
    sgl_convert_request_t *request;

    range = sgl_parallel_range_from_memory(current);
    /* SGL-MEM-DEV-001: typed conversion from threadpool cookie storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    request = (sgl_convert_request_t *)cookie;
    sgl_convert_run_range(request, range);
}

static sgl_bool_t sgl_convert_should_parallel(
    const sgl_threadpool_t *pool,
    const sgl_convert_request_t *request)
{
    sgl_bool_t result = SGL_FALSE;

#if defined(SGL_CFG_HAS_THREAD)
    sgl_uint64_t pixels;

    pixels = (sgl_uint64_t)(sgl_uint32_t)request->width *
        (sgl_uint64_t)(sgl_uint32_t)request->height;
    if ((pool != SGL_NULL) &&
        (sgl_threadpool_get_num_threads(pool) > 1U) &&
        (pixels >= SGL_CONVERT_MIN_PARALLEL_PIXELS)) {
        result = SGL_TRUE;
    }
#else
    (void)pool;
    (void)request;
#endif  /* SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_convert_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_convert_request_t *request)
{
    sgl_parallel_range_t range;
    sgl_result_t result = SGL_SUCCESS;

    if (sgl_convert_should_parallel(pool, request) == SGL_TRUE) {
        result = sgl_parallel_for(
            pool, request->height, request->row_alignment,
            sgl_convert_routine, (void *)request);
    }
    else {
        range.start = 0;
        range.count = request->height;
        sgl_convert_run_range(request, &range);
    }

    return result;
}

sgl_convert_request_t sgl_convert_make_request(
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t row_alignment,
    sgl_convert_kernel_t kernel)
{
    sgl_convert_request_t result;

    result.src = src;
    result.src_format = src_format;
    result.dst = dst;
    result.dst_format = dst_format;
    result.width = width;
    result.height = height;
    result.row_alignment = row_alignment;
    result.kernel = kernel;

    return result;
}
