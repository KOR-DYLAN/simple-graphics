#include <stdint.h>
#include <stdlib.h>
#include "sgl.h"

typedef struct {
    int32_t x1;
    int32_t x2;
    float p;
    float inv_p;
} bilinear_column_lookup_t;

typedef struct {
    int32_t y1;
    int32_t y2;
    float q;
    float inv_q;
} bilinear_row_lookup_t;

sgl_result_t sgl_generic_resize_bilinear(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    int32_t row, col;
    float rx, ry;
    int32_t x1, y1;
    int32_t x2, y2;
    float p, inv_p;
    float q, inv_q;
    float f_value;
    bilinear_column_lookup_t *column_lookup;
    bilinear_row_lookup_t *row_lookup;
    int32_t ch, src_stride;
    uint8_t *dst_buf = dst;
    uint8_t *src_y1_buf, *src_y2_buf;
    uint8_t *src_y1x1, *src_y1x2;
    uint8_t *src_y2x1, *src_y2x2;
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
        row_lookup = (bilinear_row_lookup_t *)malloc(sizeof(bilinear_row_lookup_t) * (size_t)d_width);
        column_lookup = (bilinear_column_lookup_t *)malloc(sizeof(bilinear_column_lookup_t) * (size_t)d_width);
        if ((column_lookup != NULL) && (row_lookup != NULL)) {
            /* create 'row' lookup table */
            for (row = 0; row < d_height; ++row) {
                ry = ((float)s_height - 1.f) * (float)row / ((float)d_height - 1.f);
                y1 = (int32_t)ry;
                y2 = y1 + 1;
                if (y2 == s_height) {
                    y2 = s_height - 1;
                }
                q = ry - (float)y1;

                row_lookup[row].y1 = y1;
                row_lookup[row].y2 = y2;
                row_lookup[row].q = q;
                row_lookup[row].inv_q = 1.f - q;
            }

            /* create 'column' lookup table */
            for (col = 0; col < d_width; ++col) {
                rx = ((float)s_width - 1.f) * (float)col / ((float)d_width - 1.f);
                x1 = (int32_t)rx;
                x2 = x1 + 1;
                if (x2 == s_width) {
                    x2 = s_width - 1;
                }
                p = rx - (float)x1;

                column_lookup[col].x1 = x1;
                column_lookup[col].x2 = x2;
                column_lookup[col].p = p;
                column_lookup[col].inv_p = 1.f - p;
            }

            /* resize */
            src_stride = s_width * bpp;
            for (row = 0; row < d_height; ++row) {
                y1 = row_lookup[row].y1;
                y2 = row_lookup[row].y2;
                q = row_lookup[row].q;
                inv_q = row_lookup[row].inv_q;

                src_y1_buf = src + (y1 * src_stride);
                src_y2_buf = src + (y2 * src_stride);

                for (col = 0; col < d_width; ++col) {
                    x1 = column_lookup[col].x1;
                    x2 = column_lookup[col].x2;
                    p = column_lookup[col].p;
                    inv_p = column_lookup[col].inv_p;

                    src_y1x1 = src_y1_buf + (x1 * bpp);
                    src_y1x2 = src_y1_buf + (x2 * bpp);
                    src_y2x1 = src_y2_buf + (x1 * bpp);
                    src_y2x2 = src_y2_buf + (x2 * bpp);
                    for (ch = 0; ch < bpp; ++ch) {
                        f_value =   (inv_p * inv_q * src_y1x1[ch]) +
                                    (p * inv_q * src_y1x2[ch]) +
                                    (inv_p * q * src_y2x1[ch]) +
                                    (p * q * src_y2x2[ch]);
                        dst_buf[ch] = (uint8_t)(f_value + 0.5f);
                    }
                    dst_buf += bpp;
                }
            }

            free(row_lookup);
            free(column_lookup);
        }
        else {
            SGL_SAFE_FREE(row_lookup);
            SGL_SAFE_FREE(column_lookup);
            result = SGL_ERROR_MEMORY_ALLOCATION;
        }

    }
    else {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }

    return result;
}
