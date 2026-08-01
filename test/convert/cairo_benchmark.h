/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_TEST_CAIRO_CONVERT_BENCHMARK_H_
#define SGL_TEST_CAIRO_CONVERT_BENCHMARK_H_

#include <stddef.h>
#include <stdio.h>

#include <sgl-core.h>

#include "util.h"

int sgl_test_run_cairo_convert_benchmark(
    FILE *csv,
    size_t *case_index,
    const sgl_test_png_t *image,
    sgl_fourcc_t canonical_format,
    sgl_uint8_t *src,
    sgl_uint8_t *dst);

#endif  /* SGL_TEST_CAIRO_CONVERT_BENCHMARK_H_ */
