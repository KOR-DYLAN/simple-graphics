/*
 * SGL-HDR-DEV-002: resize data types are shared by generic, SIMD, and threaded
 * implementations that intentionally use different subsets.
 */
/* cppcheck-suppress-file misra-c2012-2.3 */
/* cppcheck-suppress-file misra-c2012-2.4 */
#ifndef NEAREST_NEIGHBOR_H_
#define NEAREST_NEIGHBOR_H_

#include "sgl-fixed_point.h"

typedef struct  {
    sgl_nearest_neighbor_lookup_t *SGL_RESTRICT lut;
    uint8_t *SGL_RESTRICT src;
    uint8_t *SGL_RESTRICT dst;
    int32_t bpp;
    int32_t src_stride;
    int32_t dst_stride;
} sgl_nearest_neighbor_data_t;

typedef struct  {
    int32_t row;
    int32_t count;
} sgl_nearest_neighbor_current_t;

struct sgl_nearest_neighbor_lookup_table {
    int32_t d_width;
    int32_t d_height;
    int32_t s_width;
    int32_t s_height;
    int32_t *SGL_RESTRICT x;
    int32_t *SGL_RESTRICT y;
};

#endif  /* !NEAREST_NEIGHBOR_H_ */
