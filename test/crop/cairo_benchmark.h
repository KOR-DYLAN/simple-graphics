/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_TEST_CAIRO_CROP_BENCHMARK_H_
#define SGL_TEST_CAIRO_CROP_BENCHMARK_H_

#include <stddef.h>
#include <stdio.h>

#include "util.h"

int sgl_test_run_cairo_crop_benchmark(
    FILE *csv,
    size_t *case_index,
    const sgl_test_png_t *png);

#endif  /* SGL_TEST_CAIRO_CROP_BENCHMARK_H_ */
