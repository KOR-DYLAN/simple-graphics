/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef BICUBIC_H_
#define BICUBIC_H_

#include "sgl-fixed_point.h"
#include <sgl_memory_cast.h>

typedef struct {
    sgl_int32_t *SGL_RESTRICT x1;
    sgl_int32_t *SGL_RESTRICT x2;
    sgl_int32_t *SGL_RESTRICT x3;
    sgl_int32_t *SGL_RESTRICT x4;
    sgl_q11_t *SGL_RESTRICT p;   /* Q11 */
} bicubic_column_lookup_t;

typedef struct {
    sgl_int32_t *SGL_RESTRICT y1;
    sgl_int32_t *SGL_RESTRICT y2;
    sgl_int32_t *SGL_RESTRICT y3;
    sgl_int32_t *SGL_RESTRICT y4;
    sgl_q11_t *SGL_RESTRICT q;   /* Q11 */
} bicubic_row_lookup_t;

typedef struct  {
    sgl_bicubic_lookup_t *SGL_RESTRICT lut;
    sgl_uint8_t *SGL_RESTRICT src;
    sgl_uint8_t *SGL_RESTRICT dst;
    sgl_int32_t bpp;
    sgl_int32_t src_stride;
    sgl_int32_t dst_stride;
} sgl_bicubic_data_t;

typedef struct  {
    sgl_int32_t row;
    sgl_int32_t count;
} sgl_bicubic_current_t;

struct sgl_bicubic_lookup_table {
    sgl_int32_t d_width;
    sgl_int32_t d_height;
    sgl_int32_t s_width;
    sgl_int32_t s_height;
    bicubic_column_lookup_t col_lookup;
    bicubic_row_lookup_t row_lookup;
};

static SGL_ALWAYS_INLINE sgl_bicubic_current_t *sgl_memory_as_bicubic_current(void *memory)
{
    sgl_bicubic_current_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_bicubic_current_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE const sgl_bicubic_current_t *sgl_memory_as_const_bicubic_current(const void *memory)
{
    const sgl_bicubic_current_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (const sgl_bicubic_current_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_bicubic_data_t *sgl_memory_as_bicubic_data(void *memory)
{
    sgl_bicubic_data_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_bicubic_data_t *)memory;

    return result;
}

#endif  /* !BICUBIC_H_ */
