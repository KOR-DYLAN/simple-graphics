/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
#include <sgl-core.h>
#include "nearest_neighbor.h"

sgl_nearest_neighbor_lookup_t *sgl_generic_create_nearest_neighbor_lut(sgl_int32_t d_width, sgl_int32_t d_height, sgl_int32_t s_width, sgl_int32_t s_height)
{
    sgl_nearest_neighbor_lookup_t *lut;
    sgl_int32_t col;
    sgl_int32_t row;
    sgl_int32_t rx;
    sgl_int32_t ry;

    /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
    /* cppcheck-suppress misra-c2012-11.5 */
    lut = (sgl_nearest_neighbor_lookup_t *)sgl_malloc(sizeof(sgl_nearest_neighbor_lookup_t));
    if (lut != SGL_NULL) {
        lut->d_width = d_width;
        lut->d_height = d_height;
        lut->s_width = s_width;
        lut->s_height = s_height;
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->x = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_width);
        /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
        /* cppcheck-suppress misra-c2012-11.5 */
        lut->y = (sgl_int32_t *)sgl_malloc(sizeof(sgl_int32_t) * (sgl_size_t)d_height);

        if ((lut->x != SGL_NULL) && (lut->y != SGL_NULL)) {
            /* create 'column' lookup table */
            for (col = 0; col < d_width; ++col) {
                rx = SGL_DIV_ROUNDUP(col * (s_width - 1), d_width - 1);
                lut->x[col] = (rx >= s_width) ? (s_width - 1) : rx;
            }

            /* create 'row' lookup table */
            for (row = 0; row < d_height; ++row) {
                ry = SGL_DIV_ROUNDUP(row * (s_height - 1), d_height - 1);
                lut->y[row] = (ry >= s_height) ? (s_height - 1) : ry;
            }
        }
        else {
            SGL_SAFE_FREE(lut->x);
            SGL_SAFE_FREE(lut->y);
            SGL_SAFE_FREE(lut);
            lut = SGL_NULL;
        }
    }
    else {
        lut = SGL_NULL;
    }

    return lut;
}

void sgl_generic_destroy_nearest_neighbor_lut(sgl_nearest_neighbor_lookup_t *lut)
{
    if (lut != SGL_NULL) {
        SGL_SAFE_FREE(lut->x);
        SGL_SAFE_FREE(lut->y);
        sgl_free(lut);
    }
}
