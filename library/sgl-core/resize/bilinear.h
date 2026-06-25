/*
 * SGL-HDR-DEV-002: resize data types are shared by generic, SIMD, and threaded
 * implementations that intentionally use different subsets.
 */
/* cppcheck-suppress-file misra-c2012-2.3 */
/* cppcheck-suppress-file misra-c2012-2.4 */
#ifndef SGL_BILINEAR_H_
#define SGL_BILINEAR_H_

#include "sgl-fixed_point.h"

typedef struct {
    sgl_int32_t *SGL_RESTRICT x1;
    sgl_int32_t *SGL_RESTRICT x2;
    sgl_q11_t *SGL_RESTRICT p;
    sgl_q11_t *SGL_RESTRICT inv_p;
} bilinear_column_lookup_t;

typedef struct {
    sgl_int32_t *SGL_RESTRICT y1;
    sgl_int32_t *SGL_RESTRICT y2;
    sgl_q11_t *SGL_RESTRICT q;
    sgl_q11_t *SGL_RESTRICT inv_q;
} bilinear_row_lookup_t;

typedef struct  {
    sgl_bilinear_lookup_t *SGL_RESTRICT lut;
    sgl_uint8_t *SGL_RESTRICT src;
    sgl_uint8_t *SGL_RESTRICT dst;
    sgl_int32_t bpp;
    sgl_int32_t src_stride;
    sgl_int32_t dst_stride;
} sgl_bilinear_data_t;

typedef struct  {
    sgl_int32_t row;
    sgl_int32_t count;
} sgl_bilinear_current_t;

struct sgl_bilinear_lookup_table {
    sgl_int32_t d_width;
    sgl_int32_t d_height;
    sgl_int32_t s_width;
    sgl_int32_t s_height;
    bilinear_column_lookup_t col_lookup;
    bilinear_row_lookup_t row_lookup;
};

#endif /* SGL_BILINEAR_H_ */
