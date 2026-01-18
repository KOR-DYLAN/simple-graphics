#ifndef BICUBIC_H_
#define BICUBIC_H_

#include "sgl-fixed_point.h"

typedef struct {
    int32_t *SGL_RESTRICT x1, *SGL_RESTRICT x2, *SGL_RESTRICT x3, *SGL_RESTRICT x4;
    sgl_q11_t *SGL_RESTRICT p;   /* Q11 */
} bicubic_column_lookup_t;

typedef struct {
    int32_t *SGL_RESTRICT y1, *SGL_RESTRICT y2, *SGL_RESTRICT y3, *SGL_RESTRICT y4;
    sgl_q11_t *SGL_RESTRICT q;   /* Q11 */
} bicubic_row_lookup_t;

typedef struct  {
    sgl_bicubic_lookup_t *SGL_RESTRICT lut;
    uint8_t *SGL_RESTRICT src;
    uint8_t *SGL_RESTRICT dst;
    int32_t bpp;
    int32_t src_stride;
    int32_t dst_stride;
} sgl_bicubic_data_t;

typedef struct  {
    int32_t row;
    int32_t count;
} sgl_bicubic_current_t;

struct sgl_bicubic_lookup_table {
    int32_t d_width, d_height;
    int32_t s_width, s_height;
    bicubic_column_lookup_t col_lookup;
    bicubic_row_lookup_t row_lookup;
};

#endif  /* !BICUBIC_H_ */
