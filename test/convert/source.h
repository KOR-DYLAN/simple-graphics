/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_TEST_CONVERT_SOURCE_H
#define SGL_TEST_CONVERT_SOURCE_H

#include <stdint.h>

#include <sgl-core.h>

#include "util.h"

int sgl_test_prepare_convert_source(
    sgl_test_png_t *source,
    sgl_uint8_t *destination,
    sgl_int32_t width,
    sgl_int32_t height);

#endif /* SGL_TEST_CONVERT_SOURCE_H */
