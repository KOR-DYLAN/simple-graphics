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

#include <sgl-core.h>
#include <sgl_memory_cast.h>

#include "benchmark.h"
#if defined(SGL_TEST_HAS_CAIRO)
#include "cairo_benchmark.h"
#endif
#if defined(SGL_TEST_HAS_NE10)
#include "ne10_benchmark.h"
#endif
#include "rotate_benchmark.h"
#include "util.h"

#ifndef SGL_TEST_REPEAT_COUNT
#define SGL_TEST_REPEAT_COUNT          (10U)
#endif
#ifndef SGL_TEST_WARMUP_COUNT
#define SGL_TEST_WARMUP_COUNT          (3U)
#endif
#ifndef SGL_TEST_BENCHMARK_CSV
#define SGL_TEST_BENCHMARK_CSV         "rotate-benchmark.csv"
#endif
#ifndef SGL_TEST_OUTPUT_DIR
#define SGL_TEST_OUTPUT_DIR            "rotate-output"
#endif

#define SGL_TEST_ARRAY_SIZE(a) \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_MEMORY_POOL_SIZE       (96U * 1024U * 1024U)
#define SGL_TEST_BENCHMARK_WORKERS      (4U)

typedef sgl_result_t (*sgl_test_rotate_runner_t)(
    sgl_threadpool_t *pool,
    sgl_rotate_lookup_t *lut,
    const sgl_uint8_t *src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *dst,
    sgl_int32_t bpp,
    sgl_float64_t angle);

typedef struct {
    const char *name;
    sgl_test_rotate_runner_t run;
    sgl_bool_t use_lut;
} sgl_test_rotate_backend_t;

typedef struct {
    size_t workers;
    sgl_threadpool_t *pool;
} sgl_test_rotate_thread_t;

typedef struct {
    const char *name;
    sgl_float64_t angle;
} sgl_test_rotate_case_t;

typedef struct {
    sgl_test_rotate_runner_t run;
    sgl_threadpool_t *pool;
    sgl_rotate_lookup_t *lut;
    const sgl_test_png_t *png;
    sgl_uint8_t *dst;
    sgl_float64_t angle;
} sgl_test_rotate_context_t;

typedef struct {
    sgl_test_png_t *png;
    sgl_threadpool_t *pool;
} sgl_test_rotate_resources_t;

typedef struct {
    sgl_uint8_t *dst;
    sgl_uint8_t *reference;
    sgl_rotate_lookup_t *lut;
    sgl_size_t dst_size;
    sgl_int32_t dst_width;
    sgl_int32_t dst_height;
    sgl_bool_t reference_valid;
} sgl_test_rotate_case_resources_t;

static sgl_uint8_t sgl_test_rotate_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

static int32_t sgl_test_rotate_operation(void *context)
{
    sgl_test_rotate_context_t *rotate;
    int32_t result;

    /* cppcheck-suppress misra-c2012-11.5 */
    rotate = (sgl_test_rotate_context_t *)context;
    result = (int32_t)(rotate->run(
        rotate->pool,
        rotate->lut,
        rotate->png->data,
        rotate->png->width,
        rotate->png->height,
        rotate->dst,
        rotate->png->channels,
        rotate->angle) != SGL_SUCCESS);

    return result;
}

static int sgl_test_rotate_compare(
    const sgl_uint8_t *expected,
    const sgl_uint8_t *actual,
    sgl_size_t size)
{
    sgl_size_t index = 0U;
    int result = 0;

    while (index < size) {
        /* XOR identifies differing bits; OR retains every mismatch. */
        result |= (int)expected[index] ^ (int)actual[index];
        ++index;
    }

    return result;
}

static int sgl_test_store_rotate_output(
    size_t index,
    const sgl_test_rotate_backend_t *backend,
    const sgl_test_rotate_thread_t *thread,
    const sgl_test_rotate_case_t *rotate_case,
    const sgl_test_rotate_resources_t *resources,
    sgl_test_rotate_case_resources_t *case_resources)
{
    sgl_test_png_t output;
    char path[FILENAME_MAX];
    int path_length;
    int result = 0;

    if (case_resources->reference_valid == SGL_TRUE) {
        if (sgl_test_rotate_compare(
                case_resources->reference,
                case_resources->dst,
                case_resources->dst_size) != 0) {
            result = 1;
        }
    }
    else {
        (void)sgl_memcpy(
            case_resources->reference,
            case_resources->dst,
            case_resources->dst_size);
        case_resources->reference_valid = SGL_TRUE;
    }

    path_length = snprintf(
        path,
        sizeof(path),
        "%s/%03zu_rotate_%s_%zuthread_%s_%dx%d.png",
        SGL_TEST_OUTPUT_DIR,
        index,
        backend->name,
        thread->workers,
        rotate_case->name,
        case_resources->dst_width,
        case_resources->dst_height);
    if ((path_length < 0) || ((size_t)path_length >= sizeof(path))) {
        result = 1;
    }
    else {
        output.data = case_resources->dst;
        output.width = case_resources->dst_width;
        output.height = case_resources->dst_height;
        output.channels = resources->png->channels;
        if (sgl_test_save_png(&output, path) != 0) {
            result = 1;
        }
        else {
            (void)printf("  output[%03zu]: %s\n", index, path);
        }
    }

    return result;
}

static int sgl_test_run_rotate_case(
    FILE *csv,
    size_t index,
    const sgl_test_rotate_backend_t *backend,
    const sgl_test_rotate_thread_t *thread,
    const sgl_test_rotate_case_t *rotate_case,
    const sgl_test_rotate_resources_t *resources,
    sgl_test_rotate_case_resources_t *case_resources)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    sgl_test_rotate_context_t context;
    int result;

    context.run = backend->run;
    context.pool = thread->pool;
    context.lut = SGL_NULL;
    if (backend->use_lut == SGL_TRUE) {
        context.lut = case_resources->lut;
    }
    context.png = resources->png;
    context.dst = case_resources->dst;
    context.angle = rotate_case->angle;
    test_case.operation = rotate_case->name;
    test_case.backend = backend->name;
    test_case.workers = thread->workers;
    test_case.width = case_resources->dst_width;
    test_case.height = case_resources->dst_height;
    test_case.pixels = (uint64_t)(sgl_uint32_t)case_resources->dst_width *
        (uint64_t)(sgl_uint32_t)case_resources->dst_height;
    result = (int)sgl_test_benchmark_measure(
        sgl_test_rotate_operation,
        &context,
        SGL_TEST_WARMUP_COUNT,
        SGL_TEST_REPEAT_COUNT,
        &stats,
        samples,
        SGL_TEST_ARRAY_SIZE(samples));
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(
            csv,
            index,
            &test_case,
            &stats);
        result = sgl_test_store_rotate_output(
            index,
            backend,
            thread,
            rotate_case,
            resources,
            case_resources);
    }

    return result;
}

static sgl_threadpool_t *sgl_test_create_rotate_threadpool(int *result)
{
    sgl_threadpool_t *pool = SGL_NULL;

#if defined(SGL_CFG_HAS_THREAD)
    pool = sgl_threadpool_create(
        SGL_TEST_BENCHMARK_WORKERS,
        SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS,
        "rotate-benchmark");
    if (pool == SGL_NULL) {
        *result = 1;
    }
#else
    SGL_UNUSED_PARAM(result);
#endif

    return pool;
}

static void sgl_test_initialize_rotate_case_resources(
    sgl_test_rotate_case_resources_t *resources)
{
    resources->dst = SGL_NULL;
    resources->reference = SGL_NULL;
    resources->lut = SGL_NULL;
    resources->dst_size = 0U;
    resources->dst_width = 0;
    resources->dst_height = 0;
    resources->reference_valid = SGL_FALSE;
}

static int sgl_test_prepare_rotate_case_resources(
    const sgl_test_rotate_resources_t *resources,
    const sgl_test_rotate_case_t *rotate_case,
    sgl_test_rotate_case_resources_t *case_resources)
{
    sgl_int32_t dst_size;
    int result = 0;

    sgl_test_initialize_rotate_case_resources(case_resources);
    if (sgl_get_rotated_dimensions(
            resources->png->width,
            resources->png->height,
            rotate_case->angle,
            &case_resources->dst_width,
            &case_resources->dst_height) != SGL_SUCCESS) {
        result = 1;
    }
    dst_size = 0;
    if (result == 0) {
        dst_size = sgl_get_rotated_size(
            resources->png->width,
            resources->png->height,
            resources->png->channels,
            rotate_case->angle);
        if (dst_size <= 0) {
            result = 1;
        }
    }
    if (result == 0) {
        case_resources->dst_size = (sgl_size_t)(sgl_uint32_t)dst_size;
        case_resources->dst = sgl_memory_as_uint8(
            sgl_malloc(case_resources->dst_size));
        case_resources->reference = sgl_memory_as_uint8(
            sgl_malloc(case_resources->dst_size));
        if ((case_resources->dst == SGL_NULL) ||
            (case_resources->reference == SGL_NULL)) {
            result = 1;
        }
    }
    if (result == 0) {
        case_resources->lut = sgl_generic_create_rotate_lut(
            resources->png->width,
            resources->png->height,
            rotate_case->angle);
        if (case_resources->lut == SGL_NULL) {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_release_rotate_case_resources(
    sgl_test_rotate_case_resources_t *resources)
{
    int result = 0;

    sgl_generic_destroy_rotate_lut(resources->lut);
    if (resources->reference != SGL_NULL) {
        sgl_free(resources->reference);
    }
    if (resources->dst != SGL_NULL) {
        sgl_free(resources->dst);
    }
    sgl_test_initialize_rotate_case_resources(resources);

    return result;
}

static int sgl_test_run_rotate_case_matrix(
    FILE *csv,
    size_t *case_index,
    const sgl_test_rotate_case_t *rotate_case,
    const sgl_test_rotate_backend_t *backends,
    sgl_size_t backend_count,
    const sgl_test_rotate_thread_t *threads,
    sgl_size_t thread_count,
    const sgl_test_rotate_resources_t *resources,
    sgl_test_rotate_case_resources_t *case_resources)
{
    sgl_size_t thread_index = 0U;
    int result = 0;

    while ((result == 0) && (thread_index < thread_count)) {
        sgl_size_t backend_index = 0U;

        while ((result == 0) && (backend_index < backend_count)) {
            result = sgl_test_run_rotate_case(
                csv,
                *case_index,
                &backends[backend_index],
                &threads[thread_index],
                rotate_case,
                resources,
                case_resources);
            ++(*case_index);
            ++backend_index;
        }
        ++thread_index;
    }

    return result;
}

static int sgl_test_run_rotate_matrix(
    FILE *csv,
    const sgl_test_rotate_resources_t *resources,
    size_t *case_index)
{
    static const sgl_test_rotate_case_t rotate_cases[] = {
        {"rotate-030", 30.0},
        {"rotate-045", 45.0},
        {"rotate-060", 60.0},
        {"rotate-090", 90.0},
        {"rotate-120", 120.0},
        {"rotate-180", 180.0},
        {"rotate-240", 240.0},
        {"rotate-270", 270.0},
        {"rotate-360", 360.0}
    };
    static const sgl_test_rotate_backend_t backends[] = {
        {"generic", sgl_generic_rotate, SGL_FALSE},
        {"generic-lut", sgl_generic_rotate, SGL_TRUE},
#if defined(SGL_CFG_HAS_NEON)
        {"simd", sgl_simd_rotate, SGL_FALSE},
        {"simd-lut", sgl_simd_rotate, SGL_TRUE},
#endif
    };
    sgl_test_rotate_thread_t threads[] = {
        {1U, SGL_NULL},
#if defined(SGL_CFG_HAS_THREAD)
        {SGL_TEST_BENCHMARK_WORKERS, resources->pool},
#endif
    };
    sgl_test_rotate_case_resources_t case_resources;
    sgl_size_t rotate_index = 0U;
    int result = 0;

#if !defined(SGL_CFG_HAS_THREAD)
    SGL_UNUSED_PARAM(resources->pool);
#endif
    sgl_test_initialize_rotate_case_resources(&case_resources);
    while ((result == 0) &&
           (rotate_index < SGL_TEST_ARRAY_SIZE(rotate_cases))) {
        result = sgl_test_prepare_rotate_case_resources(
            resources,
            &rotate_cases[rotate_index],
            &case_resources);
        if (result == 0) {
            result = sgl_test_run_rotate_case_matrix(
                csv,
                case_index,
                &rotate_cases[rotate_index],
                backends,
                SGL_TEST_ARRAY_SIZE(backends),
                threads,
                SGL_TEST_ARRAY_SIZE(threads),
                resources,
                &case_resources);
        }
        result |= sgl_test_release_rotate_case_resources(&case_resources);
        ++rotate_index;
    }

    return result;
}

static int sgl_test_prepare_rotate_resources(
    const char *source_path,
    sgl_test_rotate_resources_t *resources)
{
    int result = 0;

    resources->png = sgl_test_load_png(source_path);
    resources->pool = SGL_NULL;
    if (resources->png == SGL_NULL) {
        result = 1;
    }
    if (result == 0) {
        resources->pool = sgl_test_create_rotate_threadpool(&result);
    }

    return result;
}

static int sgl_test_write_rotate_report(
    const sgl_test_rotate_resources_t *resources)
{
    FILE *csv;
    size_t case_index = 0U;
    int result = 0;

    csv = fopen(SGL_TEST_BENCHMARK_CSV, "w");
    if (csv == SGL_NULL) {
        result = 1;
    }
    if (result == 0) {
        sgl_test_benchmark_write_csv_header(csv);
        sgl_test_benchmark_print_header(
            "Rotate",
            SGL_TEST_WARMUP_COUNT,
            SGL_TEST_REPEAT_COUNT);
        result = sgl_test_run_rotate_matrix(
            csv, resources, &case_index);
#if defined(SGL_TEST_HAS_CAIRO)
        if (result == 0) {
            result = sgl_test_run_cairo_rotate_benchmark(
                csv, &case_index, resources->png);
        }
#endif
#if defined(SGL_TEST_HAS_NE10)
        if (result == 0) {
            result = sgl_test_run_ne10_rotate_benchmark(
                csv, &case_index, resources->png);
        }
#endif
        if (fclose(csv) != 0) {
            result = 1;
        }
    }
    if (result == 0) {
        (void)printf("\nbenchmark csv: %s\n", SGL_TEST_BENCHMARK_CSV);
        (void)printf("rotate output dir: %s\n", SGL_TEST_OUTPUT_DIR);
    }

    return result;
}

static int sgl_test_release_rotate_benchmark(
    sgl_test_rotate_resources_t *resources)
{
    int result = 0;

#if defined(SGL_CFG_HAS_THREAD)
    if (resources->pool != SGL_NULL) {
        if (sgl_threadpool_destroy(resources->pool) != SGL_SUCCESS) {
            result = 1;
        }
    }
#endif
    if (resources->png != SGL_NULL) {
        sgl_test_release_png(resources->png);
    }
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}

int sgl_test_run_rotate_benchmark(const char *source_path)
{
    sgl_test_rotate_resources_t resources;
    int result = 0;

    resources.png = SGL_NULL;
    resources.pool = SGL_NULL;
    if ((source_path == SGL_NULL) ||
        (sgl_memory_pool_initialize(
            sgl_test_rotate_memory_pool,
            sizeof(sgl_test_rotate_memory_pool)) != SGL_SUCCESS)) {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_prepare_rotate_resources(
            source_path,
            &resources);
    }
    if (result == 0) {
        result = sgl_test_write_rotate_report(&resources);
    }
    result |= sgl_test_release_rotate_benchmark(&resources);

    return result;
}
