/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */

#ifndef SGL_GENERIC_COMMON_H_
#define SGL_GENERIC_COMMON_H_

#include <sgl-core.h>

/* Canonical channel order used between format-specific readers and writers. */
typedef struct {
    sgl_uint8_t r;
    sgl_uint8_t g;
    sgl_uint8_t b;
    sgl_uint8_t a;
} sgl_convert_rgb_t;

/* Full-resolution YUV sample; subsampling is applied only when writing. */
typedef struct {
    sgl_uint8_t y;
    sgl_uint8_t u;
    sgl_uint8_t v;
} sgl_convert_yuv_t;

typedef struct sgl_convert_request sgl_convert_request_t;

/* A kernel receives a row range [start, start + count). */
typedef void (*sgl_convert_kernel_t)(
    sgl_convert_request_t *request,
    sgl_int32_t start,
    sgl_int32_t count);

struct sgl_convert_request {
    const sgl_uint8_t *src;
    sgl_fourcc_t src_format;
    sgl_uint8_t *dst;
    sgl_fourcc_t dst_format;
    sgl_int32_t width;
    sgl_int32_t height;
    /* 4:2:0 kernels require ranges aligned to a two-row chroma group. */
    sgl_int32_t row_alignment;
    sgl_convert_kernel_t kernel;
};

/* Destination chroma group clipped at the right and bottom image edges. */
typedef struct {
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t width;
    sgl_int32_t height;
} sgl_convert_block_t;

/* Format validation and checked buffer-size calculation. */
sgl_bool_t sgl_convert_is_rgb_format(sgl_fourcc_t format);
sgl_bool_t sgl_convert_is_yuv_format(sgl_fourcc_t format);
sgl_result_t sgl_convert_validate_common(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst,
    sgl_int32_t width,
    sgl_int32_t height);
sgl_result_t sgl_convert_validate_buffer_sizes(
    sgl_fourcc_t src_format,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);
sgl_size_t sgl_convert_buffer_size(
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height);
sgl_uint8_t sgl_convert_average_chroma(
    sgl_int32_t sum,
    sgl_int32_t count);

/* Shared Q8 arithmetic used by both RGB-to-YUV and YUV-to-RGB equations. */
sgl_uint8_t sgl_convert_clip_int32(sgl_int32_t value);
sgl_int32_t sgl_convert_divide_256(sgl_int32_t value);

/* Format-independent pixel access over packed and planar layouts. */
sgl_convert_rgb_t sgl_convert_read_rgb(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y);
void sgl_convert_write_rgb(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_rgb_t rgb);
sgl_convert_yuv_t sgl_convert_read_yuv(
    const sgl_uint8_t *src,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y);
void sgl_convert_write_yuv_luma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_uint8_t value);
void sgl_convert_write_yuv_chroma(
    sgl_uint8_t *dst,
    sgl_fourcc_t format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_convert_yuv_t value);

/* Chroma group geometry and row-range execution shared by YUV writers. */
sgl_int32_t sgl_convert_yuv_group_width(sgl_fourcc_t format);
sgl_int32_t sgl_convert_yuv_group_height(sgl_fourcc_t format);
sgl_convert_block_t sgl_convert_make_block(
    const sgl_convert_request_t *request,
    sgl_int32_t x,
    sgl_int32_t y);
sgl_convert_request_t sgl_convert_make_request(
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_int32_t row_alignment,
    sgl_convert_kernel_t kernel);
sgl_result_t sgl_convert_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_convert_request_t *request);

#endif  /* SGL_GENERIC_COMMON_H_ */
