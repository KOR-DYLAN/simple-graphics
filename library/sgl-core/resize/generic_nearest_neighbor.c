#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"

sgl_result_t sgl_generic_resize_nearest(uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row, col;
    int32_t rx, ry;
    int32_t x, y, ch;
    int32_t *x_lookup;
    int32_t src_stride;
    uint8_t *dst_buf = dst, *src_buf, *src_row_buf;
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

    if (errcnt == 0) {
        x_lookup = (int32_t *)malloc(sizeof(int32_t) * (size_t)d_width);
        if (x_lookup != NULL) {
            /* create 'column' lookup table */
            for (col = 0; col < d_width; ++col) {
                rx = SGL_DIV_ROUNDUP(col * (s_width - 1), d_width - 1);
                x_lookup[col] = (rx >= s_width) ? (s_width - 1) : rx;
            }
            
            /* resize */
            src_stride = s_width * bpp;
            for (row = 0; row < d_height; ++row) {
                ry = SGL_DIV_ROUNDUP(row * (s_height - 1), d_height - 1);
                y = (ry >= s_height) ? (s_height - 1) : ry;
                src_row_buf = (src + (src_stride * y));
        
                for (col = 0; col < d_width; ++col) {
                    x = x_lookup[col];
                    src_buf = src_row_buf + (x * bpp);
                    for (ch = 0; ch < bpp; ++ch) {
                        dst_buf[ch] = src_buf[ch];
                    }
        
                    dst_buf += bpp;
                }
            }

            free(x_lookup);
        }
        else {
            result = SGL_ERROR_MEMORY_ALLOCATION;
        }
    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}
