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
#include "convert_benchmark.h"
#include "source.h"
#include "util.h"

#ifndef SGL_TEST_REPEAT_COUNT
#define SGL_TEST_REPEAT_COUNT          (10U)
#endif
#ifndef SGL_TEST_WARMUP_COUNT
#define SGL_TEST_WARMUP_COUNT          (3U)
#endif
#ifndef SGL_TEST_BENCHMARK_CSV
#define SGL_TEST_BENCHMARK_CSV         "convert-benchmark.csv"
#endif
#ifndef SGL_TEST_CONVERT_MATRIX_WIDTH
#define SGL_TEST_CONVERT_MATRIX_WIDTH  (1280)
#endif
#ifndef SGL_TEST_CONVERT_MATRIX_HEIGHT
#define SGL_TEST_CONVERT_MATRIX_HEIGHT (720)
#endif

#define SGL_TEST_ARRAY_SIZE(a) \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_MEMORY_POOL_SIZE       (64U * 1024U * 1024U)
#define SGL_TEST_BENCHMARK_WORKERS      (4U)
#define SGL_TEST_OPERATION_CAPACITY     (64U)

typedef sgl_result_t (*sgl_test_convert_runner_t)(
    sgl_threadpool_t *pool,
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);

typedef enum {
    SGL_TEST_CONVERT_RGB = 0,
    SGL_TEST_CONVERT_YUV = 1
} sgl_test_convert_family_t;

typedef enum {
    SGL_TEST_CONVERT_BACKEND_GENERIC,
    SGL_TEST_CONVERT_BACKEND_SIMD
} sgl_test_convert_backend_id_t;

typedef struct {
    const char *name;
    sgl_fourcc_t fourcc;
    sgl_test_convert_family_t family;
} sgl_test_convert_format_t;

typedef struct {
    sgl_test_convert_runner_t generic_runner;
    sgl_test_convert_runner_t simd_runner;
} sgl_test_convert_operation_t;

typedef struct {
    const char *name;
    sgl_test_convert_backend_id_t id;
} sgl_test_convert_backend_t;

typedef struct {
    size_t workers;
    sgl_threadpool_t *pool;
} sgl_test_convert_thread_t;

typedef struct {
    sgl_test_convert_runner_t run;
    sgl_threadpool_t *pool;
    const sgl_uint8_t *src;
    sgl_fourcc_t src_format;
    sgl_uint8_t *dst;
    sgl_fourcc_t dst_format;
    sgl_int32_t width;
    sgl_int32_t height;
} sgl_test_convert_context_t;

typedef struct {
    sgl_test_png_t *png;
    sgl_test_png_t image;
    sgl_threadpool_t *pool;
    sgl_uint8_t *src;
    sgl_uint8_t *dst;
    sgl_fourcc_t canonical_format;
} sgl_test_convert_resources_t;

static const sgl_test_convert_format_t sgl_test_convert_formats[] = {
    {"rgba", SGL_FOURCC_RGBA, SGL_TEST_CONVERT_RGB},
    {"argb", SGL_FOURCC_ARGB, SGL_TEST_CONVERT_RGB},
    {"bgra", SGL_FOURCC_BGRA, SGL_TEST_CONVERT_RGB},
    {"abgr", SGL_FOURCC_ABGR, SGL_TEST_CONVERT_RGB},
    {"rgb", SGL_FOURCC_RGB, SGL_TEST_CONVERT_RGB},
    {"bgr", SGL_FOURCC_BGR, SGL_TEST_CONVERT_RGB},
    {"rgba-planar", SGL_FOURCC_RGBA_PLANAR, SGL_TEST_CONVERT_RGB},
    {"argb-planar", SGL_FOURCC_ARGB_PLANAR, SGL_TEST_CONVERT_RGB},
    {"bgra-planar", SGL_FOURCC_BGRA_PLANAR, SGL_TEST_CONVERT_RGB},
    {"abgr-planar", SGL_FOURCC_ABGR_PLANAR, SGL_TEST_CONVERT_RGB},
    {"rgb-planar", SGL_FOURCC_RGB_PLANAR, SGL_TEST_CONVERT_RGB},
    {"bgr-planar", SGL_FOURCC_BGR_PLANAR, SGL_TEST_CONVERT_RGB},
    {"yuv444-packed", SGL_FOURCC_YUV444_PACKED, SGL_TEST_CONVERT_YUV},
    {"yuv444-planar", SGL_FOURCC_YUV444_PLANAR, SGL_TEST_CONVERT_YUV},
    {"yuyv", SGL_FOURCC_YUYV, SGL_TEST_CONVERT_YUV},
    {"yvyu", SGL_FOURCC_YVYU, SGL_TEST_CONVERT_YUV},
    {"uyvy", SGL_FOURCC_UYVY, SGL_TEST_CONVERT_YUV},
    {"vyuy", SGL_FOURCC_VYUY, SGL_TEST_CONVERT_YUV},
    {"yuv422-planar", SGL_FOURCC_YUV422_PLANAR, SGL_TEST_CONVERT_YUV},
    {"yuv420-planar", SGL_FOURCC_YUV420_PLANAR, SGL_TEST_CONVERT_YUV},
    {"nv12", SGL_FOURCC_NV12, SGL_TEST_CONVERT_YUV},
    {"nv21", SGL_FOURCC_NV21, SGL_TEST_CONVERT_YUV}
};

static const sgl_test_convert_operation_t sgl_test_convert_operations[] = {
    {
        sgl_generic_rgb2rgb,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_rgb2rgb
#else
        SGL_NULL
#endif
    },
    {
        sgl_generic_rgb2yuv,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_rgb2yuv
#else
        SGL_NULL
#endif
    },
    {
        sgl_generic_yuv2rgb,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_yuv2rgb
#else
        SGL_NULL
#endif
    },
    {
        sgl_generic_yuv2yuv,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_yuv2yuv
#else
        SGL_NULL
#endif
    }
};

static const sgl_test_convert_backend_t sgl_test_convert_backends[] = {
    {"generic", SGL_TEST_CONVERT_BACKEND_GENERIC},
#if defined(SGL_CFG_HAS_NEON)
    {"simd", SGL_TEST_CONVERT_BACKEND_SIMD}
#endif
};

static sgl_uint8_t sgl_test_convert_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

static int32_t sgl_test_convert_operation(void *context)
{
    sgl_test_convert_context_t *convert;
    int32_t result;

    /* cppcheck-suppress misra-c2012-11.5 */
    convert = (sgl_test_convert_context_t *)context;
    result = (int32_t)(convert->run(
        convert->pool,
        convert->src,
        convert->src_format,
        convert->dst,
        convert->dst_format,
        convert->width,
        convert->height) != SGL_SUCCESS);

    return result;
}

static sgl_fourcc_t sgl_test_convert_canonical_format(
    sgl_int32_t channels)
{
    sgl_fourcc_t result;

    switch (channels) {
    case 3:
        result = SGL_FOURCC_RGB;
        break;
    case 4:
        result = SGL_FOURCC_RGBA;
        break;
    default:
        result = 0U;
        break;
    }

    return result;
}

static int sgl_test_convert_capacity(
    const sgl_test_png_t *png,
    sgl_size_t *capacity)
{
    int result = 0;

    if ((png == SGL_NULL) || (png->width <= 0) || (png->height <= 0) ||
        (capacity == SGL_NULL)) {
        result = 1;
    }
    if ((result == 0) &&
        ((((sgl_uint32_t)png->width | (sgl_uint32_t)png->height) & 1U) !=
         0U)) {
        /* A set low bit in either dimension is invalid for 4:2:0 formats. */
        result = 1;
    }
    if ((result == 0) &&
        ((sgl_size_t)(sgl_uint32_t)png->width >
         (SGL_SIZE_MAX / (sgl_size_t)(sgl_uint32_t)png->height))) {
        result = 1;
    }
    if (result == 0) {
        sgl_size_t pixels;

        pixels = (sgl_size_t)(sgl_uint32_t)png->width *
            (sgl_size_t)(sgl_uint32_t)png->height;
        /* Right shift by two tests room for four bytes per pixel. */
        if (pixels > (SGL_SIZE_MAX >> 2U)) {
            result = 1;
        }
        else {
            /* Left shift by two allocates four bytes per pixel. */
            *capacity = pixels << 2U;
        }
    }

    return result;
}

static const sgl_test_convert_operation_t *sgl_test_get_operation(
    sgl_test_convert_family_t src_family,
    sgl_test_convert_family_t dst_family)
{
    const sgl_test_convert_operation_t *result;
    sgl_uint32_t mode;

    /* Source is bit 1 and destination is bit 0: RR, RY, YR, YY. */
    mode = ((sgl_uint32_t)src_family << 1U) |
        (sgl_uint32_t)dst_family;
    switch (mode) {
    case 0U:
        result = &sgl_test_convert_operations[0];
        break;
    case 1U:
        result = &sgl_test_convert_operations[1];
        break;
    case 2U:
        result = &sgl_test_convert_operations[2];
        break;
    default:
        result = &sgl_test_convert_operations[3];
        break;
    }

    return result;
}

static sgl_test_convert_runner_t sgl_test_get_runner(
    const sgl_test_convert_operation_t *operation,
    sgl_test_convert_backend_id_t backend)
{
    sgl_test_convert_runner_t result;

    switch (backend) {
    case SGL_TEST_CONVERT_BACKEND_GENERIC:
        result = operation->generic_runner;
        break;
    case SGL_TEST_CONVERT_BACKEND_SIMD:
        result = operation->simd_runner;
        break;
    default:
        result = SGL_NULL;
        break;
    }

    return result;
}

static int sgl_test_append_operation_text(
    char *operation,
    sgl_size_t capacity,
    sgl_size_t *offset,
    const char *text)
{
    sgl_size_t index = 0U;
    int result = 0;

    if ((operation == SGL_NULL) || (offset == SGL_NULL) ||
        (text == SGL_NULL) || (*offset >= capacity)) {
        result = 1;
    }
    while ((result == 0) && (text[index] != '\0')) {
        if ((*offset + 1U) >= capacity) {
            result = 1;
        }
        else {
            operation[*offset] = text[index];
            ++(*offset);
            ++index;
        }
    }
    if (result == 0) {
        operation[*offset] = '\0';
    }

    return result;
}

static int sgl_test_build_operation_name(
    const sgl_test_convert_format_t *src_format,
    const sgl_test_convert_format_t *dst_format,
    char *operation,
    sgl_size_t capacity)
{
    sgl_size_t offset = 0U;
    int result;

    result = sgl_test_append_operation_text(
        operation, capacity, &offset, src_format->name);
    if (result == 0) {
        result = sgl_test_append_operation_text(
            operation, capacity, &offset, "-to-");
    }
    if (result == 0) {
        result = sgl_test_append_operation_text(
            operation, capacity, &offset, dst_format->name);
    }

    return result;
}

static int sgl_test_prepare_source_format(
    const sgl_test_convert_format_t *format,
    sgl_test_convert_resources_t *resources)
{
    sgl_result_t convert_result;
    int result;

    switch (format->family) {
    case SGL_TEST_CONVERT_RGB:
        convert_result = sgl_generic_rgb2rgb(
            SGL_NULL,
            resources->image.data,
            resources->canonical_format,
            resources->src,
            format->fourcc,
            resources->image.width,
            resources->image.height);
        break;
    case SGL_TEST_CONVERT_YUV:
        convert_result = sgl_generic_rgb2yuv(
            SGL_NULL,
            resources->image.data,
            resources->canonical_format,
            resources->src,
            format->fourcc,
            resources->image.width,
            resources->image.height);
        break;
    default:
        convert_result = SGL_ERROR_NOT_SUPPORTED;
        break;
    }
    result = (int)(convert_result != SGL_SUCCESS);

    return result;
}

static int sgl_test_run_convert_case(
    FILE *csv,
    size_t index,
    const char *operation,
    const sgl_test_convert_backend_t *backend,
    const sgl_test_convert_thread_t *thread,
    sgl_test_convert_context_t *context)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    int result;

    context->pool = thread->pool;
    test_case.operation = operation;
    test_case.backend = backend->name;
    test_case.workers = thread->workers;
    test_case.width = context->width;
    test_case.height = context->height;
    test_case.pixels = (uint64_t)(sgl_uint32_t)context->width *
        (uint64_t)(sgl_uint32_t)context->height;
    result = (int)sgl_test_benchmark_measure(
        sgl_test_convert_operation,
        context,
        SGL_TEST_WARMUP_COUNT,
        SGL_TEST_REPEAT_COUNT,
        &stats,
        samples,
        SGL_TEST_ARRAY_SIZE(samples));
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(
            csv, index, &test_case, &stats);
    }

    return result;
}

static int sgl_test_run_backend_cases(
    FILE *csv,
    size_t *case_index,
    const char *operation_name,
    const sgl_test_convert_operation_t *operation,
    const sgl_test_convert_backend_t *backend,
    sgl_threadpool_t *pool,
    sgl_test_convert_context_t *context)
{
    sgl_test_convert_thread_t threads[] = {
        {1U, SGL_NULL},
#if defined(SGL_CFG_HAS_THREAD)
        {SGL_TEST_BENCHMARK_WORKERS, pool}
#endif
    };
    sgl_size_t thread_index = 0U;
    int result = 0;

#if !defined(SGL_CFG_HAS_THREAD)
    SGL_UNUSED_PARAM(pool);
#endif
    context->run = sgl_test_get_runner(operation, backend->id);
    if (context->run == SGL_NULL) {
        result = 1;
    }
    while (thread_index < SGL_TEST_ARRAY_SIZE(threads)) {
        if (context->run != SGL_NULL) {
            /* Bitwise OR preserves failures while completing every case. */
            result |= sgl_test_run_convert_case(
                csv,
                *case_index,
                operation_name,
                backend,
                &threads[thread_index],
                context);
        }
        ++(*case_index);
        ++thread_index;
    }

    return result;
}

static int sgl_test_run_format_pair(
    FILE *csv,
    size_t *case_index,
    const sgl_test_convert_format_t *src_format,
    const sgl_test_convert_format_t *dst_format,
    sgl_threadpool_t *pool,
    sgl_test_convert_context_t *context)
{
    const sgl_test_convert_operation_t *operation;
    char operation_name[SGL_TEST_OPERATION_CAPACITY];
    sgl_size_t backend_index = 0U;
    int name_result;
    int result = 0;

    operation = sgl_test_get_operation(
        src_format->family, dst_format->family);
    context->dst_format = dst_format->fourcc;
    name_result = sgl_test_build_operation_name(
        src_format,
        dst_format,
        operation_name,
        SGL_TEST_ARRAY_SIZE(operation_name));
    if (name_result != 0) {
        result = 1;
    }
    while (backend_index < SGL_TEST_ARRAY_SIZE(
            sgl_test_convert_backends)) {
        if (name_result == 0) {
            /* Bitwise OR keeps later backends available after a failure. */
            result |= sgl_test_run_backend_cases(
                csv,
                case_index,
                operation_name,
                operation,
                &sgl_test_convert_backends[backend_index],
                pool,
                context);
        }
        ++backend_index;
    }

    return result;
}

static int sgl_test_run_source_format(
    FILE *csv,
    size_t *case_index,
    const sgl_test_convert_format_t *src_format,
    sgl_test_convert_resources_t *resources,
    sgl_test_convert_context_t *context)
{
    sgl_size_t dst_index = 0U;
    int source_result;
    int result = 0;

    source_result = sgl_test_prepare_source_format(src_format, resources);
    if (source_result != 0) {
        result = 1;
    }
    context->src_format = src_format->fourcc;
    while (dst_index < SGL_TEST_ARRAY_SIZE(sgl_test_convert_formats)) {
        if (source_result == 0) {
            /* Bitwise OR retains a failure while testing all destinations. */
            result |= sgl_test_run_format_pair(
                csv,
                case_index,
                src_format,
                &sgl_test_convert_formats[dst_index],
                resources->pool,
                context);
        }
        ++dst_index;
    }

    return result;
}

static int sgl_test_run_convert_matrix(
    FILE *csv,
    sgl_test_convert_resources_t *resources,
    size_t *case_index)
{
    sgl_test_convert_context_t context;
    sgl_size_t src_index = 0U;
    int result = 0;

    context.run = sgl_generic_rgb2rgb;
    context.pool = SGL_NULL;
    context.src = resources->src;
    context.src_format = SGL_FOURCC_RGBA;
    context.dst = resources->dst;
    context.dst_format = SGL_FOURCC_RGBA;
    context.width = resources->image.width;
    context.height = resources->image.height;
    while (src_index < SGL_TEST_ARRAY_SIZE(sgl_test_convert_formats)) {
        /* Bitwise OR keeps every source format available for diagnostics. */
        result |= sgl_test_run_source_format(
            csv,
            case_index,
            &sgl_test_convert_formats[src_index],
            resources,
            &context);
        ++src_index;
    }

    return result;
}

static sgl_threadpool_t *sgl_test_create_convert_threadpool(int *result)
{
    sgl_threadpool_t *pool = SGL_NULL;

#if defined(SGL_CFG_HAS_THREAD)
    pool = sgl_threadpool_create(
        SGL_TEST_BENCHMARK_WORKERS,
        SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS,
        "convert-benchmark");
    if (pool == SGL_NULL) {
        *result = 1;
    }
#else
    SGL_UNUSED_PARAM(result);
#endif

    return pool;
}

static int sgl_test_prepare_convert_resources(
    const char *source_path,
    sgl_test_convert_resources_t *resources)
{
    sgl_size_t capacity = 0U;
    int result = 0;

    resources->png = sgl_test_load_png(source_path);
    resources->image.data = SGL_NULL;
    resources->image.width = SGL_TEST_CONVERT_MATRIX_WIDTH;
    resources->image.height = SGL_TEST_CONVERT_MATRIX_HEIGHT;
    resources->image.channels = 0;
    resources->pool = SGL_NULL;
    resources->src = SGL_NULL;
    resources->dst = SGL_NULL;
    resources->canonical_format = 0U;
    if (resources->png == SGL_NULL) {
        result = 1;
    }
    if (result == 0) {
        resources->image.channels = resources->png->channels;
        resources->canonical_format = sgl_test_convert_canonical_format(
            resources->image.channels);
        if (resources->canonical_format == 0U) {
            result = 1;
        }
    }
    if (result == 0) {
        result = sgl_test_convert_capacity(&resources->image, &capacity);
    }
    if (result == 0) {
        resources->image.data = sgl_memory_as_uint8(sgl_malloc(capacity));
        resources->src = sgl_memory_as_uint8(sgl_malloc(capacity));
        resources->dst = sgl_memory_as_uint8(sgl_malloc(capacity));
        if ((resources->image.data == SGL_NULL) ||
            (resources->src == SGL_NULL) ||
            (resources->dst == SGL_NULL)) {
            result = 1;
        }
    }
    if (result == 0) {
        result = sgl_test_prepare_convert_source(
            resources->png,
            resources->image.data,
            resources->image.width,
            resources->image.height);
    }
    if (result == 0) {
        resources->pool = sgl_test_create_convert_threadpool(&result);
    }

    return result;
}

static int sgl_test_write_convert_report(
    sgl_test_convert_resources_t *resources)
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
            "Convert", SGL_TEST_WARMUP_COUNT, SGL_TEST_REPEAT_COUNT);
        result = sgl_test_run_convert_matrix(
            csv, resources, &case_index);
#if defined(SGL_TEST_HAS_CAIRO)
        if (result == 0) {
            result = sgl_test_run_cairo_convert_benchmark(
                csv,
                &case_index,
                &resources->image,
                resources->canonical_format,
                resources->src,
                resources->dst);
        }
#endif
        if (fclose(csv) != 0) {
            result = 1;
        }
    }
    if (result == 0) {
        (void)printf("\nbenchmark csv: %s\n", SGL_TEST_BENCHMARK_CSV);
        if (fflush(stdout) != 0) {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_release_convert_benchmark(
    sgl_test_convert_resources_t *resources)
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
    if (resources->src != SGL_NULL) {
        sgl_free(resources->src);
    }
    if (resources->image.data != SGL_NULL) {
        sgl_free(resources->image.data);
    }
    if (resources->png != SGL_NULL) {
        sgl_test_release_png(resources->png);
    }
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}

int sgl_test_run_convert_benchmark(const char *source_path)
{
    sgl_test_convert_resources_t resources;
    int result = 0;

    resources.png = SGL_NULL;
    resources.image.data = SGL_NULL;
    resources.image.width = 0;
    resources.image.height = 0;
    resources.image.channels = 0;
    resources.pool = SGL_NULL;
    resources.src = SGL_NULL;
    resources.dst = SGL_NULL;
    resources.canonical_format = 0U;
    if ((source_path == SGL_NULL) ||
        (sgl_memory_pool_initialize(
            sgl_test_convert_memory_pool,
            sizeof(sgl_test_convert_memory_pool)) != SGL_SUCCESS)) {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_prepare_convert_resources(
            source_path, &resources);
    }
    if (result == 0) {
        result = sgl_test_write_convert_report(&resources);
    }
    result |= sgl_test_release_convert_benchmark(&resources);

    return result;
}
