#ifndef NEAREST_NEIGHBOR_H_
#define NEAREST_NEIGHBOR_H_

#include "sgl-fixed_point.h"
#include <sgl_memory_cast.h>

typedef struct  {
    sgl_nearest_neighbor_lookup_t *SGL_RESTRICT lut;
    sgl_uint8_t *SGL_RESTRICT src;
    sgl_uint8_t *SGL_RESTRICT dst;
    sgl_int32_t bpp;
    sgl_int32_t src_stride;
    sgl_int32_t dst_stride;
} sgl_nearest_neighbor_data_t;

typedef struct  {
    sgl_int32_t row;
    sgl_int32_t count;
} sgl_nearest_neighbor_current_t;

struct sgl_nearest_neighbor_lookup_table {
    sgl_int32_t d_width;
    sgl_int32_t d_height;
    sgl_int32_t s_width;
    sgl_int32_t s_height;
    sgl_int32_t *SGL_RESTRICT x;
    sgl_int32_t *SGL_RESTRICT y;
};

static SGL_ALWAYS_INLINE sgl_nearest_neighbor_current_t *sgl_memory_as_nearest_neighbor_current(void *memory)
{
    sgl_nearest_neighbor_current_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_nearest_neighbor_current_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE const sgl_nearest_neighbor_current_t *sgl_memory_as_const_nearest_neighbor_current(const void *memory)
{
    const sgl_nearest_neighbor_current_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (const sgl_nearest_neighbor_current_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_nearest_neighbor_data_t *sgl_memory_as_nearest_neighbor_data(void *memory)
{
    sgl_nearest_neighbor_data_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_nearest_neighbor_data_t *)memory;

    return result;
}

#endif  /* !NEAREST_NEIGHBOR_H_ */
