/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>

#include "source.h"

int sgl_test_prepare_convert_source(
    sgl_test_png_t *source,
    sgl_uint8_t *destination,
    sgl_int32_t width,
    sgl_int32_t height)
{
    sgl_result_t resize_result = SGL_FAILURE;
    int result = 0;

    if ((source == SGL_NULL) ||
        (source->data == SGL_NULL) ||
        (destination == SGL_NULL) ||
        (width <= 0) ||
        (height <= 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (source->channels != 3) &&
        (source->channels != 4)) {
        result = 1;
    }
    if (result == 0) {
        resize_result = sgl_generic_resize_bilinear(
            SGL_NULL,
            SGL_NULL,
            destination,
            width,
            height,
            source->data,
            source->width,
            source->height,
            source->channels);
        result = (int)(resize_result != SGL_SUCCESS);
    }

    return result;
}
