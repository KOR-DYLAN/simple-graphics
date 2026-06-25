/*
 * SGL-HDR-DEV-002: resize data types are shared by generic, SIMD, and threaded
 * implementations that intentionally use different subsets.
 */
/* cppcheck-suppress-file misra-c2012-2.3 */
/* cppcheck-suppress-file misra-c2012-2.4 */
#ifndef BICUBIC_H_
#define BICUBIC_H_

#include "sgl-fixed_point.h"

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

#endif  /* !BICUBIC_H_ */
