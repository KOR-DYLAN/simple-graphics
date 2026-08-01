/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#ifndef SGL_NEON_COMMON_H_
#define SGL_NEON_COMMON_H_

#include <arm_neon.h>

#include <sgl-core.h>

/* Every conversion kernel processes eight 8-bit pixels per NEON vector. */
#define SGL_NEON_LANES    (8U)

/* Canonical eight-pixel RGB vector independent of source channel order. */
typedef struct {
    uint8x8_t r;
    uint8x8_t g;
    uint8x8_t b;
    uint8x8_t a;
} sgl_neon_rgb_t;

/* Eight full-resolution YUV samples before destination subsampling. */
typedef struct {
    uint8x8_t y;
    uint8x8_t u;
    uint8x8_t v;
} sgl_neon_yuv_t;

/* Contiguous-pixel kernel used by RGB and YUV444 conversion paths. */
typedef void (*sgl_neon_convert_kernel_t)(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_size_t pixels,
    sgl_size_t start,
    sgl_size_t count);

typedef struct {
    const sgl_uint8_t *src;
    sgl_fourcc_t src_format;
    sgl_uint8_t *dst;
    sgl_fourcc_t dst_format;
    sgl_size_t pixels;
    sgl_neon_convert_kernel_t kernel;
} sgl_neon_convert_data_t;

typedef struct sgl_neon_yuv_data sgl_neon_yuv_data_t;

/* Row kernel used when chroma planes require width and row coordinates. */
typedef void (*sgl_neon_yuv_kernel_t)(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t start,
    sgl_int32_t count);

struct sgl_neon_yuv_data {
    const sgl_uint8_t *src;
    sgl_fourcc_t src_format;
    sgl_uint8_t *dst;
    sgl_fourcc_t dst_format;
    sgl_int32_t width;
    sgl_int32_t height;
    sgl_size_t pixels;
    /* 4:2:0 stores consume two rows; other paths use one-row alignment. */
    sgl_int32_t row_alignment;
    sgl_neon_yuv_kernel_t kernel;
};

/* Format predicates, argument checks, and canonical vector access. */
sgl_bool_t sgl_neon_is_rgb(sgl_fourcc_t format);
sgl_bool_t sgl_neon_is_yuv444(sgl_fourcc_t format);
sgl_bool_t sgl_neon_is_yuv422(sgl_fourcc_t format);
sgl_bool_t sgl_neon_is_yuv420(sgl_fourcc_t format);
sgl_result_t sgl_neon_validate(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst,
    sgl_int32_t width,
    sgl_int32_t height);
sgl_result_t sgl_neon_get_pixel_count(
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_size_t *pixels);
sgl_neon_rgb_t sgl_neon_load_rgb(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel);
void sgl_neon_store_rgb(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel,
    sgl_neon_rgb_t value);
sgl_neon_yuv_t sgl_neon_load_yuv444(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel);
void sgl_neon_store_yuv444(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_size_t pixels,
    sgl_size_t pixel,
    sgl_neon_yuv_t value);
int16x8_t sgl_neon_widen_u8_to_s16(uint8x8_t value);

/* Subsampled YUV access duplicates shared chroma into eight logical lanes. */
sgl_neon_yuv_t sgl_neon_yuv_load(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x);
void sgl_neon_yuv_store_422(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x,
    sgl_neon_yuv_t value);
void sgl_neon_yuv_store_420(
    const sgl_neon_yuv_data_t *data,
    sgl_int32_t row,
    sgl_int32_t x,
    sgl_neon_yuv_t top,
    sgl_neon_yuv_t bottom);

/* Threadpool dispatch for contiguous pixels and row-addressed YUV data. */
sgl_neon_convert_data_t sgl_neon_convert_make_data(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_fourcc_t dst_format,
    sgl_size_t pixels,
    sgl_neon_convert_kernel_t kernel);
sgl_result_t sgl_neon_convert_execute(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_neon_convert_data_t *data);
sgl_result_t sgl_neon_yuv_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t row_alignment,
    sgl_neon_yuv_kernel_t kernel);

#endif  /* SGL_NEON_COMMON_H_ */
