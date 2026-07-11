#ifndef SGL_BILINEAR_H_
#define SGL_BILINEAR_H_

#include "sgl-fixed_point.h"
#include <sgl_memory_cast.h>

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

static SGL_ALWAYS_INLINE sgl_bilinear_current_t *sgl_memory_as_bilinear_current(void *memory)
{
    sgl_bilinear_current_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_bilinear_current_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE const sgl_bilinear_current_t *sgl_memory_as_const_bilinear_current(const void *memory)
{
    const sgl_bilinear_current_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (const sgl_bilinear_current_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_bilinear_data_t *sgl_memory_as_bilinear_data(void *memory)
{
    sgl_bilinear_data_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_bilinear_data_t *)memory;

    return result;
}

#endif /* SGL_BILINEAR_H_ */
