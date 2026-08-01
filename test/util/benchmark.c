/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <stdint.h>
#include <stdio.h>

#include "benchmark.h"
#include "util.h"

#define SGL_TEST_MICROSECONDS_PER_SECOND    (1000000ULL)

static void sgl_test_benchmark_stats_initialize(
    sgl_test_benchmark_stats_t *stats,
    uint64_t *samples,
    size_t sample_capacity)
{
    stats->min_us = UINT64_MAX;
    stats->max_us = 0ULL;
    stats->total_us = 0ULL;
    stats->samples = samples;
    stats->sample_capacity = sample_capacity;
    stats->sample_count = 0U;
}

static int32_t sgl_test_benchmark_add_sample(
    sgl_test_benchmark_stats_t *stats,
    uint64_t elapsed_us)
{
    int32_t result = 0;

    if (stats->sample_count >= stats->sample_capacity) {
        result = 1;
    }
    if (result == 0) {
        size_t index;

        stats->total_us += elapsed_us;
        if (elapsed_us < stats->min_us) {
            stats->min_us = elapsed_us;
        }
        if (elapsed_us > stats->max_us) {
            stats->max_us = elapsed_us;
        }

        index = stats->sample_count;
        while ((index > 0U) &&
               (stats->samples[index - 1U] > elapsed_us)) {
            stats->samples[index] = stats->samples[index - 1U];
            --index;
        }
        stats->samples[index] = elapsed_us;
        ++stats->sample_count;
    }

    return result;
}

int32_t sgl_test_benchmark_measure(
    sgl_test_benchmark_runner_t runner,
    void *context,
    size_t warmup_count,
    size_t repeat_count,
    sgl_test_benchmark_stats_t *stats,
    uint64_t *samples,
    size_t sample_capacity)
{
    size_t index;
    int32_t result = 0;

    if ((runner == NULL) || (stats == NULL) || (samples == NULL) ||
        (repeat_count == 0U) || (repeat_count > sample_capacity)) {
        result = 1;
    }
    index = 0U;
    while ((result == 0) && (index < warmup_count)) {
        result = runner(context);
        ++index;
    }
    if (result == 0) {
        sgl_test_benchmark_stats_initialize(
            stats, samples, sample_capacity);
    }
    index = 0U;
    while ((result == 0) && (index < repeat_count)) {
        uint64_t start_us;
        uint64_t elapsed_us;

        start_us = sgl_test_get_timestamp_us(0ULL);
        result = runner(context);
        elapsed_us = sgl_test_get_timestamp_us(start_us);
        if (result == 0) {
            result = sgl_test_benchmark_add_sample(stats, elapsed_us);
        }
        ++index;
    }

    return result;
}

uint64_t sgl_test_benchmark_average_us(
    const sgl_test_benchmark_stats_t *stats)
{
    uint64_t result = 0ULL;

    if ((stats != NULL) && (stats->sample_count > 0U)) {
        result = stats->total_us / (uint64_t)stats->sample_count;
    }

    return result;
}

uint64_t sgl_test_benchmark_median_us(
    const sgl_test_benchmark_stats_t *stats)
{
    uint64_t result = 0ULL;

    if ((stats != NULL) && (stats->sample_count > 0U)) {
        size_t middle;

        /* Right shift by one selects the middle index of sorted samples. */
        middle = stats->sample_count >> 1U;
        result = stats->samples[middle];
        /* A cleared low bit means the sample count is even. */
        if ((stats->sample_count & 1U) == 0U) {
            result = stats->samples[middle - 1U] +
                ((stats->samples[middle] -
                  stats->samples[middle - 1U]) >> 1U);
        }
    }

    return result;
}

uint64_t sgl_test_benchmark_pixels_per_second(
    const sgl_test_benchmark_case_t *test_case,
    const sgl_test_benchmark_stats_t *stats)
{
    uint64_t median_us;
    uint64_t result = 0ULL;

    median_us = sgl_test_benchmark_median_us(stats);
    if ((test_case != NULL) && (median_us > 0ULL) &&
        (test_case->pixels <=
         (UINT64_MAX / SGL_TEST_MICROSECONDS_PER_SECOND))) {
        result = (test_case->pixels * SGL_TEST_MICROSECONDS_PER_SECOND) /
            median_us;
    }

    return result;
}

void sgl_test_benchmark_print_header(
    const char *title,
    size_t warmup_count,
    size_t repeat_count)
{
    (void)printf("\n%s benchmark\n", title);
    (void)printf("warm-up count: %zu\n", warmup_count);
    (void)printf("repeat count: %zu\n", repeat_count);
    (void)printf(
        "%4s  %-31s  %-11s  %2s  %-11s  %12s  %12s  %12s  %12s  %12s\n",
        "case", "operation", "backend", "th", "size",
        "median(us)", "avg(us)", "min(us)", "max(us)", "pixels/s");
}

void sgl_test_benchmark_print_result(
    size_t index,
    const sgl_test_benchmark_case_t *test_case,
    const sgl_test_benchmark_stats_t *stats)
{
    (void)printf(
        "%4zu  %-31s  %-11s  %2zu  %5dx%-5d  "
        "%12llu  %12llu  %12llu  %12llu  %12llu\n",
        index,
        test_case->operation,
        test_case->backend,
        test_case->workers,
        test_case->width,
        test_case->height,
        (unsigned long long)sgl_test_benchmark_median_us(stats),
        (unsigned long long)sgl_test_benchmark_average_us(stats),
        (unsigned long long)stats->min_us,
        (unsigned long long)stats->max_us,
        (unsigned long long)sgl_test_benchmark_pixels_per_second(
            test_case, stats));
}

void sgl_test_benchmark_write_csv_header(FILE *csv)
{
    (void)fprintf(
        csv,
        "case,operation,backend,threads,width,height,pixels,"
        "avg_us,min_us,max_us,median_us,pixels_per_second\n");
}

void sgl_test_benchmark_write_csv_result(
    FILE *csv,
    size_t index,
    const sgl_test_benchmark_case_t *test_case,
    const sgl_test_benchmark_stats_t *stats)
{
    (void)fprintf(
        csv,
        "%zu,%s,%s,%zu,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu\n",
        index,
        test_case->operation,
        test_case->backend,
        test_case->workers,
        test_case->width,
        test_case->height,
        (unsigned long long)test_case->pixels,
        (unsigned long long)sgl_test_benchmark_average_us(stats),
        (unsigned long long)stats->min_us,
        (unsigned long long)stats->max_us,
        (unsigned long long)sgl_test_benchmark_median_us(stats),
        (unsigned long long)sgl_test_benchmark_pixels_per_second(
            test_case, stats));
}
