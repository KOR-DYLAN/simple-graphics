/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_CROP_COMMON_H_
#define SGL_CROP_COMMON_H_

#include <sgl-core.h>

/* Backend row copier; size is the exact number of cropped bytes per row. */
typedef void (*sgl_crop_copy_row_t)(
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_size_t size);

sgl_result_t sgl_crop_validate(
    const sgl_uint8_t *src,
    const sgl_uint8_t *dst,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp);

/*
 * Executes independent crop rows serially or through the threadpool. The
 * caller supplies the generic or NEON row-copy implementation.
 */
sgl_result_t sgl_crop_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    const sgl_uint8_t *SGL_RESTRICT src,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t src_width,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp,
    sgl_crop_copy_row_t copy_row);

#endif  /* SGL_CROP_COMMON_H_ */
