/* SGL-C89-DEV-001: declarations remain at block start for C89 compatibility. */
/* cppcheck-suppress-file variableScope */
/* SGL-CALLBACK-DEV-001: thread callbacks recover typed context from void *. */
/* cppcheck-suppress-file misra-c2012-11.5 */
/* cppcheck-suppress-file constParameterCallback */
#include <sgl-core.h>
#include "nearest_neighbor.h"

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static SGL_ALWAYS_INLINE void sgl_generic_resize_nearest_neighbor_line_stripe(sgl_int32_t row, sgl_nearest_neighbor_data_t *data) {
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    sgl_int32_t col;
    sgl_int32_t ch;
    sgl_int32_t d_width;
    sgl_int32_t bpp;
    const sgl_int32_t *x;
    sgl_uint8_t *src_y_buf;
    const sgl_uint8_t *src;
    sgl_uint8_t *dst;

    d_width = lut->d_width;
    bpp = data->bpp;
    x = lut->x;
    src_y_buf = &data->src[lut->y[row] * data->src_stride];
    dst = &data->dst[row * data->dst_stride];

    for (col = 0; col < d_width; ++col) {
        src = &src_y_buf[x[col] * bpp];
        switch (bpp) {
        case 4:
            dst[3] = src[3];
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        case 3:
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        case 2:
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        case 1:
            dst[0] = src[0];
            break;
        default:
            for (ch = 0; ch < bpp; ++ch) {
                dst[ch] = src[ch];
            }
            break;
        }
        dst = &dst[bpp];
    }
}

sgl_result_t sgl_generic_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_int32_t row;
    sgl_nearest_neighbor_data_t data;
    sgl_nearest_neighbor_lookup_t *lut = SGL_NULL;
    sgl_nearest_neighbor_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;
    sgl_size_t copy_size;

    /* check buffer address */
    if ((dst == SGL_NULL) || (src == SGL_NULL)) {
        errcnt += 1;
    }

    /* check boundary */
    if ((d_width <= 0) || (d_height <= 0) || (s_width <= 0) || (s_height <= 0)) {
        errcnt += 1;
    }

    /* check bpp(bytes per pixel) */
    if (bpp <= 0) {
        errcnt += 1;
    }

    /* check error count */
    if ((errcnt == 0) && (d_width == s_width) && (d_height == s_height)) {
        copy_size = (sgl_size_t)d_width * (sgl_size_t)d_height * (sgl_size_t)bpp;
        if (dst != src) {
            (void)sgl_memcpy(dst, src, copy_size);
        }
    }
    else if (errcnt == 0) {
         if (ext_lut != SGL_NULL) {
            if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
                (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
            {
                /* apply external look-up table */
                lut = ext_lut;
            }
        }

        if (lut == SGL_NULL) {
            /* create temp look-up table */
            temp_lut = sgl_generic_create_nearest_neighbor_lut(d_width, d_height, s_width, s_height);
            lut = temp_lut;
        }

        if (lut != SGL_NULL) {
            /* set data */
            data.bpp = bpp;
            data.src = src;
            data.dst = dst;
            data.lut = lut;
            data.src_stride = s_width * bpp;
            data.dst_stride = d_width * bpp;

            if (pool == SGL_NULL) {
                /* single-threaded resize */
                for (row = 0; row < d_height; ++row) {
                    sgl_generic_resize_nearest_neighbor_line_stripe(row, (void *)&data);
                }
            }
#if defined(SGL_CFG_HAS_THREAD)
            else {
                sgl_nearest_neighbor_current_t *currents;
                sgl_queue_t *operations = SGL_NULL;
                sgl_int32_t i;
                sgl_int32_t num_operations;
                sgl_int32_t mod_operations;

                num_operations = d_height / SGL_GENERIC_BULK_SIZE;
                mod_operations = d_height % SGL_GENERIC_BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((sgl_size_t)num_operations);
                /* SGL-MEM-DEV-001: typed conversion from the generic allocator. */
                /* cppcheck-suppress misra-c2012-11.5 */
                currents = (sgl_nearest_neighbor_current_t *)sgl_malloc(sizeof(sgl_nearest_neighbor_current_t) * (sgl_size_t)num_operations);
                if ((operations != SGL_NULL) && (currents != SGL_NULL)) {
                    for (i = 0; i < num_operations; ++i) {
                        currents[i].row = i * SGL_GENERIC_BULK_SIZE;
                        currents[i].count = SGL_GENERIC_BULK_SIZE;
                        (void)sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
                    }

                    if (mod_operations != 0) {
                        currents[num_operations - 1].count = mod_operations;
                    }

                    /* multi-threaded resize */
                    (void)sgl_threadpool_attach_routine(pool, sgl_generic_resize_nearest_neighbor_routine, operations, (void *)&data);
                    sgl_queue_destroy(&operations);
                }
                else {
                    result = SGL_ERROR_MEMORY_ALLOCATION;
                }

                SGL_SAFE_FREE(currents);
                SGL_SAFE_FREE(operations);
            }
#else
            else {
                result = SGL_ERROR_NOT_SUPPORTED;
            }
#endif  /* !SGL_CFG_HAS_THREAD */

            if (temp_lut != SGL_NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_nearest_neighbor_lut(temp_lut);
            }
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_nearest_neighbor_current_t *cur = (const sgl_nearest_neighbor_current_t *)current;
    sgl_nearest_neighbor_data_t *data = (sgl_nearest_neighbor_data_t *)cookie;
    sgl_int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_generic_resize_nearest_neighbor_line_stripe(row, data);
    }
}
#endif  /* !SGL_CFG_HAS_THREAD */
