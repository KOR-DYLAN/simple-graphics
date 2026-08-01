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
#include "crop_benchmark.h"
#include "util.h"

#ifndef SGL_TEST_REPEAT_COUNT
#define SGL_TEST_REPEAT_COUNT          (10U)
#endif
#ifndef SGL_TEST_WARMUP_COUNT
#define SGL_TEST_WARMUP_COUNT          (3U)
#endif
#ifndef SGL_TEST_BENCHMARK_CSV
#define SGL_TEST_BENCHMARK_CSV         "crop-benchmark.csv"
#endif
#ifndef SGL_TEST_OUTPUT_DIR
#define SGL_TEST_OUTPUT_DIR            "crop-output"
#endif

#define SGL_TEST_ARRAY_SIZE(a) \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_MEMORY_POOL_SIZE       (64U * 1024U * 1024U)
#define SGL_TEST_BENCHMARK_WORKERS      (4U)
#define SGL_TEST_CROP_CASE_COUNT        (8U)

typedef sgl_result_t (*sgl_test_crop_runner_t)(
    sgl_threadpool_t *pool,
    const sgl_uint8_t *src,
    sgl_int32_t src_width,
    sgl_int32_t src_height,
    sgl_uint8_t *dst,
    sgl_int32_t x,
    sgl_int32_t y,
    sgl_int32_t crop_width,
    sgl_int32_t crop_height,
    sgl_int32_t bpp);

typedef struct {
    const char *name;
    sgl_test_crop_runner_t run;
} sgl_test_crop_backend_t;

typedef struct {
    size_t workers;
    sgl_threadpool_t *pool;
} sgl_test_crop_thread_t;

typedef struct {
    const char *name;
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t width;
    sgl_int32_t height;
} sgl_test_crop_case_t;

typedef struct {
    sgl_test_crop_runner_t run;
    sgl_threadpool_t *pool;
    const sgl_test_png_t *png;
    sgl_uint8_t *dst;
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t width;
    sgl_int32_t height;
} sgl_test_crop_context_t;

typedef struct {
    sgl_test_png_t *png;
    sgl_threadpool_t *pool;
    sgl_uint8_t *dst;
    sgl_uint8_t *reference;
    sgl_size_t dst_size;
} sgl_test_crop_resources_t;

static sgl_uint8_t sgl_test_crop_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

static int32_t sgl_test_crop_operation(void *context)
{
    sgl_test_crop_context_t *crop;
    int32_t result;

    /* cppcheck-suppress misra-c2012-11.5 */
    crop = (sgl_test_crop_context_t *)context;
    result = (int32_t)(crop->run(
        crop->pool,
        crop->png->data,
        crop->png->width,
        crop->png->height,
        crop->dst,
        crop->x,
        crop->y,
        crop->width,
        crop->height,
        crop->png->channels) != SGL_SUCCESS);

    return result;
}

static int sgl_test_crop_image_size(
    const sgl_test_png_t *png,
    sgl_int32_t width,
    sgl_int32_t height,
    sgl_size_t *size)
{
    int result = 0;

    if ((png == SGL_NULL) || (width <= 0) || (height <= 0) ||
        (png->channels <= 0) || (size == SGL_NULL)) {
        result = 1;
    }
    if ((result == 0) &&
        ((sgl_size_t)(sgl_uint32_t)width >
         (SGL_SIZE_MAX / (sgl_size_t)(sgl_uint32_t)height))) {
        result = 1;
    }
    if (result == 0) {
        sgl_size_t pixels;

        pixels = (sgl_size_t)(sgl_uint32_t)width *
            (sgl_size_t)(sgl_uint32_t)height;
        if (pixels >
            (SGL_SIZE_MAX / (sgl_size_t)(sgl_uint32_t)png->channels)) {
            result = 1;
        }
        else {
            *size = pixels *
                (sgl_size_t)(sgl_uint32_t)png->channels;
        }
    }

    return result;
}

static int sgl_test_crop_compare(
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

static int sgl_test_store_crop_output(
    size_t index,
    const sgl_test_crop_backend_t *backend,
    const sgl_test_crop_thread_t *thread,
    const sgl_test_crop_case_t *crop_case,
    sgl_test_crop_resources_t *resources,
    sgl_bool_t *reference_valid)
{
    sgl_test_png_t output;
    char path[FILENAME_MAX];
    int path_length;
    int result = 0;

    if (*reference_valid == SGL_TRUE) {
        if (sgl_test_crop_compare(
                resources->reference,
                resources->dst,
                resources->dst_size) != 0) {
            result = 1;
        }
    }
    else {
        (void)sgl_memcpy(
            resources->reference,
            resources->dst,
            resources->dst_size);
        *reference_valid = SGL_TRUE;
    }

    path_length = snprintf(
        path,
        sizeof(path),
        "%s/%03zu_crop_%s_%zuthread_%s_x%d_y%d_%dx%d.png",
        SGL_TEST_OUTPUT_DIR,
        index,
        backend->name,
        thread->workers,
        crop_case->name,
        crop_case->x,
        crop_case->y,
        crop_case->width,
        crop_case->height);
    if ((path_length < 0) || ((size_t)path_length >= sizeof(path))) {
        result = 1;
    }
    else {
        output.data = resources->dst;
        output.width = crop_case->width;
        output.height = crop_case->height;
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

static int sgl_test_run_crop_case(
    FILE *csv,
    size_t index,
    const sgl_test_crop_backend_t *backend,
    const sgl_test_crop_thread_t *thread,
    const sgl_test_crop_case_t *crop_case,
    sgl_test_crop_resources_t *resources,
    sgl_bool_t *reference_valid)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    sgl_test_crop_context_t context;
    int result;

    context.run = backend->run;
    context.pool = thread->pool;
    context.png = resources->png;
    context.dst = resources->dst;
    context.x = crop_case->x;
    context.y = crop_case->y;
    context.width = crop_case->width;
    context.height = crop_case->height;
    test_case.operation = crop_case->name;
    test_case.backend = backend->name;
    test_case.workers = thread->workers;
    test_case.width = crop_case->width;
    test_case.height = crop_case->height;
    test_case.pixels = (uint64_t)(sgl_uint32_t)crop_case->width *
        (uint64_t)(sgl_uint32_t)crop_case->height;
    result = (int)sgl_test_benchmark_measure(
        sgl_test_crop_operation,
        &context,
        SGL_TEST_WARMUP_COUNT,
        SGL_TEST_REPEAT_COUNT,
        &stats,
        samples,
        SGL_TEST_ARRAY_SIZE(samples));
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(
            csv, index, &test_case, &stats);
        resources->dst_size = (sgl_size_t)test_case.pixels *
            (sgl_size_t)(sgl_uint32_t)resources->png->channels;
        result = sgl_test_store_crop_output(
            index,
            backend,
            thread,
            crop_case,
            resources,
            reference_valid);
    }

    return result;
}

static void sgl_test_initialize_crop_cases(
    const sgl_test_png_t *png,
    sgl_test_crop_case_t *cases)
{
    sgl_int32_t half_width;
    sgl_int32_t half_height;
    sgl_int32_t quarter_width;
    sgl_int32_t quarter_height;

    half_width = png->width >> 1U;
    half_height = png->height >> 1U;
    quarter_width = png->width >> 2U;
    quarter_height = png->height >> 2U;

    cases[0] = (sgl_test_crop_case_t){
        "full-frame", 0, 0, png->width, png->height};
    cases[1] = (sgl_test_crop_case_t){
        "top-left-quarter", 0, 0, half_width, half_height};
    cases[2] = (sgl_test_crop_case_t){
        "top-right-quarter",
        png->width - half_width,
        0,
        half_width,
        half_height};
    cases[3] = (sgl_test_crop_case_t){
        "center-half",
        (png->width - half_width) >> 1U,
        (png->height - half_height) >> 1U,
        half_width,
        half_height};
    cases[4] = (sgl_test_crop_case_t){
        "bottom-left-quarter",
        0,
        png->height - half_height,
        half_width,
        half_height};
    cases[5] = (sgl_test_crop_case_t){
        "bottom-right-quarter",
        png->width - half_width,
        png->height - half_height,
        half_width,
        half_height};
    cases[6] = (sgl_test_crop_case_t){
        "center-wide",
        png->width >> 3U,
        (png->height - quarter_height) >> 1U,
        png->width - (png->width >> 2U),
        quarter_height};
    cases[7] = (sgl_test_crop_case_t){
        "center-tall",
        (png->width - quarter_width) >> 1U,
        png->height >> 3U,
        quarter_width,
        png->height - (png->height >> 2U)};
}

static int sgl_test_run_crop_matrix(
    FILE *csv,
    sgl_test_crop_resources_t *resources,
    size_t *case_index)
{
    static const sgl_test_crop_backend_t backends[] = {
        {"generic", sgl_generic_crop},
#if defined(SGL_CFG_HAS_NEON)
        {"simd", sgl_simd_crop},
#endif
    };
    sgl_test_crop_thread_t threads[] = {
        {1U, SGL_NULL},
#if defined(SGL_CFG_HAS_THREAD)
        {SGL_TEST_BENCHMARK_WORKERS, resources->pool},
#endif
    };
    sgl_test_crop_case_t crop_cases[SGL_TEST_CROP_CASE_COUNT];
    sgl_size_t crop_index = 0U;
    int result = 0;

#if !defined(SGL_CFG_HAS_THREAD)
    SGL_UNUSED_PARAM(resources->pool);
#endif
    sgl_test_initialize_crop_cases(resources->png, crop_cases);
    while ((result == 0) &&
           (crop_index < SGL_TEST_ARRAY_SIZE(crop_cases))) {
        sgl_size_t thread_index = 0U;
        sgl_bool_t reference_valid = SGL_FALSE;

        while ((result == 0) &&
               (thread_index < SGL_TEST_ARRAY_SIZE(threads))) {
            sgl_size_t backend_index = 0U;

            while ((result == 0) &&
                   (backend_index < SGL_TEST_ARRAY_SIZE(backends))) {
                result = sgl_test_run_crop_case(
                    csv,
                    *case_index,
                    &backends[backend_index],
                    &threads[thread_index],
                    &crop_cases[crop_index],
                    resources,
                    &reference_valid);
                ++(*case_index);
                ++backend_index;
            }
            ++thread_index;
        }
        ++crop_index;
    }

    return result;
}

static sgl_threadpool_t *sgl_test_create_crop_threadpool(int *result)
{
    sgl_threadpool_t *pool = SGL_NULL;

#if defined(SGL_CFG_HAS_THREAD)
    pool = sgl_threadpool_create(
        SGL_TEST_BENCHMARK_WORKERS,
        SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS,
        "crop-benchmark");
    if (pool == SGL_NULL) {
        *result = 1;
    }
#else
    SGL_UNUSED_PARAM(result);
#endif

    return pool;
}

static int sgl_test_prepare_crop_resources(
    const char *source_path,
    sgl_test_crop_resources_t *resources)
{
    sgl_size_t dst_size = 0U;
    int result = 0;

    resources->png = sgl_test_load_png(source_path);
    resources->pool = SGL_NULL;
    resources->dst = SGL_NULL;
    resources->reference = SGL_NULL;
    resources->dst_size = 0U;
    if (resources->png == SGL_NULL) {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_crop_image_size(
            resources->png,
            resources->png->width,
            resources->png->height,
            &dst_size);
    }
    if (result == 0) {
        resources->dst = sgl_memory_as_uint8(sgl_malloc(dst_size));
        if (resources->dst == SGL_NULL) {
            result = 1;
        }
    }
    if (result == 0) {
        resources->reference = sgl_memory_as_uint8(sgl_malloc(dst_size));
        if (resources->reference == SGL_NULL) {
            result = 1;
        }
    }
    if (result == 0) {
        resources->pool = sgl_test_create_crop_threadpool(&result);
    }

    return result;
}

static int sgl_test_write_crop_report(
    sgl_test_crop_resources_t *resources)
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
            "Crop", SGL_TEST_WARMUP_COUNT, SGL_TEST_REPEAT_COUNT);
        result = sgl_test_run_crop_matrix(
            csv, resources, &case_index);
#if defined(SGL_TEST_HAS_CAIRO)
        if (result == 0) {
            result = sgl_test_run_cairo_crop_benchmark(
                csv, &case_index, resources->png);
        }
#endif
        if (fclose(csv) != 0) {
            result = 1;
        }
    }
    if (result == 0) {
        (void)printf("\nbenchmark csv: %s\n", SGL_TEST_BENCHMARK_CSV);
        (void)printf("crop output dir: %s\n", SGL_TEST_OUTPUT_DIR);
    }

    return result;
}

static int sgl_test_release_crop_benchmark(
    sgl_test_crop_resources_t *resources)
{
    int result = 0;

#if defined(SGL_CFG_HAS_THREAD)
    if (resources->pool != SGL_NULL) {
        if (sgl_threadpool_destroy(resources->pool) != SGL_SUCCESS) {
            result = 1;
        }
    }
#endif
    if (resources->dst != SGL_NULL) {
        sgl_free(resources->dst);
    }
    if (resources->reference != SGL_NULL) {
        sgl_free(resources->reference);
    }
    if (resources->png != SGL_NULL) {
        sgl_test_release_png(resources->png);
    }
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}

int sgl_test_run_crop_benchmark(const char *source_path)
{
    sgl_test_crop_resources_t resources;
    int result = 0;

    resources.png = SGL_NULL;
    resources.pool = SGL_NULL;
    resources.dst = SGL_NULL;
    resources.reference = SGL_NULL;
    resources.dst_size = 0U;
    if ((source_path == SGL_NULL) ||
        (sgl_memory_pool_initialize(
            sgl_test_crop_memory_pool,
            sizeof(sgl_test_crop_memory_pool)) != SGL_SUCCESS)) {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_prepare_crop_resources(
            source_path, &resources);
    }
    if (result == 0) {
        result = sgl_test_write_crop_report(&resources);
    }
    result |= sgl_test_release_crop_benchmark(&resources);

    return result;
}
