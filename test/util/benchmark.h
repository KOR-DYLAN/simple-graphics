/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_TEST_BENCHMARK_H_
#define SGL_TEST_BENCHMARK_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int32_t (*sgl_test_benchmark_runner_t)(void *context);

typedef struct {
    uint64_t min_us;
    uint64_t max_us;
    uint64_t total_us;
    uint64_t *samples;
    size_t sample_capacity;
    size_t sample_count;
} sgl_test_benchmark_stats_t;

typedef struct {
    const char *operation;
    const char *backend;
    size_t workers;
    int32_t width;
    int32_t height;
    uint64_t pixels;
} sgl_test_benchmark_case_t;

int32_t sgl_test_benchmark_measure(
    sgl_test_benchmark_runner_t runner,
    void *context,
    size_t warmup_count,
    size_t repeat_count,
    sgl_test_benchmark_stats_t *stats,
    uint64_t *samples,
    size_t sample_capacity);

uint64_t sgl_test_benchmark_average_us(
    const sgl_test_benchmark_stats_t *stats);
uint64_t sgl_test_benchmark_median_us(
    const sgl_test_benchmark_stats_t *stats);
uint64_t sgl_test_benchmark_pixels_per_second(
    const sgl_test_benchmark_case_t *test_case,
    const sgl_test_benchmark_stats_t *stats);

void sgl_test_benchmark_print_header(
    const char *title,
    size_t warmup_count,
    size_t repeat_count);
void sgl_test_benchmark_print_result(
    size_t index,
    const sgl_test_benchmark_case_t *test_case,
    const sgl_test_benchmark_stats_t *stats);
void sgl_test_benchmark_write_csv_header(FILE *csv);
void sgl_test_benchmark_write_csv_result(
    FILE *csv,
    size_t index,
    const sgl_test_benchmark_case_t *test_case,
    const sgl_test_benchmark_stats_t *stats);

#endif  /* SGL_TEST_BENCHMARK_H_ */
