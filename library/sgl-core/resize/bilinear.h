#ifndef SGL_BILINEAR_H_
#define SGL_BILINEAR_H_

#include "sgl-fixed_point.h"

typedef struct {
    int32_t *SGL_RESTRICT x1, *SGL_RESTRICT x2;
    sgl_q11_t *SGL_RESTRICT p, *SGL_RESTRICT inv_p;   /* Q11 */
} bilinear_column_lookup_t;

typedef struct {
    int32_t *SGL_RESTRICT y1, *SGL_RESTRICT y2;
    sgl_q11_t *SGL_RESTRICT q, *SGL_RESTRICT inv_q;   /* Q11 */
} bilinear_row_lookup_t;

typedef struct  {
    sgl_bilinear_lookup_t *SGL_RESTRICT lut;
    uint8_t *SGL_RESTRICT src;
    uint8_t *SGL_RESTRICT dst;
    int32_t bpp;
    int32_t src_stride;
    int32_t dst_stride;
} sgl_bilinear_data_t;

typedef struct  {
    int32_t row;
    int32_t count;
} sgl_bilinear_current_t;

struct sgl_bilinear_lookup_table {
    int32_t d_width, d_height;
    int32_t s_width, s_height;
    bilinear_column_lookup_t col_lookup;
    bilinear_row_lookup_t row_lookup;
};

#endif /* SGL_BILINEAR_H_ */
