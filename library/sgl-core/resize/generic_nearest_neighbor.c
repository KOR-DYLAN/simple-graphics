#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"
#include "nearest_neighbor.h"

static void sgl_generic_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);

static SGL_ALWAYS_INLINE void sgl_generic_resize_nearest_neighbor_line_stripe(int32_t row, sgl_nearest_neighbor_data_t *data) {
    sgl_nearest_neighbor_lookup_t *lut = data->lut;
    int32_t col, ch;
    int32_t d_width, bpp;
    int32_t *x;
    uint8_t *src_y_buf, *src, *dst;

    d_width = lut->d_width;
    bpp = data->bpp;
    x = lut->x;
    src_y_buf = data->src + (lut->y[row] * data->src_stride);
    dst = data->dst + (row * data->dst_stride);
    
    for (col = 0; col < d_width; ++col) {
        src = src_y_buf + (x[col] * bpp);
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
            /* Not Supported */
            break;
        }
        for (ch = 0; ch < bpp; ++ch) {
            dst[ch] = src[ch];
        }
        dst += bpp;
    }
}

sgl_result_t sgl_generic_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row;
    sgl_nearest_neighbor_current_t *currents;
    sgl_nearest_neighbor_data_t data;
    sgl_queue_t *operations = NULL;
    sgl_nearest_neighbor_lookup_t *lut = NULL, *temp_lut = NULL;
    int32_t i, num_operations, mod_operations;
    int32_t errcnt = 0;

    /* check buffer address */
    if ((dst == NULL) || (src == NULL)) {
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
    if (errcnt == 0) {
         if (ext_lut != NULL) {
            if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
                (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
            {
                /* apply external look-up table */
                lut = ext_lut;
            }
        }

        if (lut == NULL) {
            /* create temp look-up table */
            temp_lut = sgl_generic_create_nearest_neighbor_lut(d_width, d_height, s_width, s_height);
            lut = temp_lut;
        }

        if (lut != NULL) {
            /* set data */
            data.bpp = bpp;
            data.src = src;
            data.dst = dst;
            data.lut = lut;
            data.src_stride = s_width * bpp;
            data.dst_stride = d_width * bpp;

            if (pool == NULL) {
                /* single-threaded resize */
                for (row = 0; row < d_height; ++row) {
                    sgl_generic_resize_nearest_neighbor_line_stripe(row, (void *)&data);
                }
            }
            else {
                num_operations = d_height / SGL_GENERIC_BULK_SIZE;
                mod_operations = d_height % SGL_GENERIC_BULK_SIZE;
                if (mod_operations != 0) {
                    num_operations += 1;
                }

                operations = sgl_queue_create((size_t)num_operations);
                currents = (sgl_nearest_neighbor_current_t *)malloc(sizeof(sgl_nearest_neighbor_current_t) * (size_t)num_operations);
                if ((operations != NULL) && (currents != NULL)) {
                    for (i = 0; i < num_operations; ++i) {
                        currents[i].row = i * SGL_GENERIC_BULK_SIZE;
                        currents[i].count = SGL_GENERIC_BULK_SIZE;
                        sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
                    }

                    if (mod_operations != 0) {
                        currents[num_operations - 1].count = mod_operations;
                    }

                    /* multi-threaded resize */
                    sgl_threadpool_attach_routine(pool, sgl_generic_resize_nearest_neighbor_routine, operations, (void *)&data);
                    sgl_queue_destroy(&operations);
                }
                else {
                    result = SGL_ERROR_MEMORY_ALLOCATION;
                }

                SGL_SAFE_FREE(currents);
                SGL_SAFE_FREE(operations);
            }

            if (temp_lut != NULL) {
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

static void sgl_generic_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    sgl_nearest_neighbor_current_t *cur = (sgl_nearest_neighbor_current_t *)current;
    sgl_nearest_neighbor_data_t *data = (sgl_nearest_neighbor_data_t *)cookie;
    int32_t row;

    for (row = cur->row; row < (cur->row + cur->count); ++row) {
        sgl_generic_resize_nearest_neighbor_line_stripe(row, data);
    }
}
