/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <NE10_imgproc.h>

#include <stdint.h>
#include <stdio.h>

#include <sgl-core.h>
#include <sgl_memory_cast.h>

#include "benchmark.h"
#include "ne10_benchmark.h"

#ifndef SGL_TEST_REPEAT_COUNT
#define SGL_TEST_REPEAT_COUNT          (10U)
#endif
#ifndef SGL_TEST_WARMUP_COUNT
#define SGL_TEST_WARMUP_COUNT          (3U)
#endif
#ifndef SGL_TEST_OUTPUT_DIR
#define SGL_TEST_OUTPUT_DIR            "rotate-output"
#endif

#define SGL_TEST_ARRAY_SIZE(a) \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_NE10_RGBA_BYTES       (4U)

typedef struct {
    const char *name;
    ne10_int32_t angle;
} sgl_test_ne10_rotate_case_t;

typedef struct {
    const sgl_test_png_t *png;
    sgl_uint8_t *dst;
    sgl_size_t capacity;
    ne10_int32_t angle;
    ne10_uint32_t width;
    ne10_uint32_t height;
} sgl_test_ne10_rotate_context_t;

static int32_t sgl_test_ne10_rotate_operation(void *context)
{
    sgl_test_ne10_rotate_context_t *rotate;
    ne10_uint32_t width = 0U;
    ne10_uint32_t height = 0U;
    int32_t result = 0;

    /* cppcheck-suppress misra-c2012-11.5 */
    rotate = (sgl_test_ne10_rotate_context_t *)context;
    ne10_img_rotate_rgba_c(
        (ne10_uint8_t *)rotate->dst,
        &width,
        &height,
        (ne10_uint8_t *)rotate->png->data,
        (ne10_uint32_t)rotate->png->width,
        (ne10_uint32_t)rotate->png->height,
        rotate->angle);
    if ((width != rotate->width) || (height != rotate->height)) {
        result = 1;
    }

    return result;
}

static sgl_size_t sgl_test_ne10_rotate_capacity(
    const sgl_test_png_t *png)
{
    sgl_size_t side;
    sgl_size_t pixels = 0U;
    sgl_size_t result = 0U;

    side = (sgl_size_t)(sgl_uint32_t)png->width +
        (sgl_size_t)(sgl_uint32_t)png->height + 1U;
    if (side <= (SGL_SIZE_MAX / side)) {
        pixels = side * side;
        if (pixels <= (SGL_SIZE_MAX / SGL_TEST_NE10_RGBA_BYTES)) {
            result = pixels * SGL_TEST_NE10_RGBA_BYTES;
        }
    }

    return result;
}

static int sgl_test_ne10_prepare_case(
    const sgl_test_ne10_rotate_case_t *rotate_case,
    sgl_test_ne10_rotate_context_t *context)
{
    sgl_size_t output_size;
    int result = 0;

    context->angle = rotate_case->angle;
    context->width = 0U;
    context->height = 0U;
    (void)sgl_memset(context->dst, 0, context->capacity);
    ne10_img_rotate_rgba_c(
        (ne10_uint8_t *)context->dst,
        &context->width,
        &context->height,
        (ne10_uint8_t *)context->png->data,
        (ne10_uint32_t)context->png->width,
        (ne10_uint32_t)context->png->height,
        context->angle);
    output_size = (sgl_size_t)context->width *
        (sgl_size_t)context->height * SGL_TEST_NE10_RGBA_BYTES;
    if ((context->width == 0U) ||
        (context->height == 0U) ||
        (output_size > context->capacity)) {
        result = 1;
    }

    return result;
}

static int sgl_test_ne10_store_output(
    size_t index,
    const sgl_test_ne10_rotate_case_t *rotate_case,
    const sgl_test_ne10_rotate_context_t *context)
{
    sgl_test_png_t output;
    char path[FILENAME_MAX];
    int path_length;
    int result = 0;

    path_length = snprintf(
        path,
        sizeof(path),
        "%s/%03zu_rotate_ne10-c-bilinear_1thread_%s_%ux%u.png",
        SGL_TEST_OUTPUT_DIR,
        index,
        rotate_case->name,
        context->width,
        context->height);
    if ((path_length < 0) || ((size_t)path_length >= sizeof(path))) {
        result = 1;
    }
    else {
        output.data = context->dst;
        output.width = (sgl_int32_t)context->width;
        output.height = (sgl_int32_t)context->height;
        output.channels = (sgl_int32_t)SGL_TEST_NE10_RGBA_BYTES;
        if (sgl_test_save_png(&output, path) != 0) {
            result = 1;
        }
        else {
            (void)printf("  output[%03zu]: %s\n", index, path);
        }
    }

    return result;
}

static int sgl_test_run_ne10_rotate_case(
    FILE *csv,
    size_t index,
    const sgl_test_ne10_rotate_case_t *rotate_case,
    sgl_test_ne10_rotate_context_t *context)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    int result;

    result = sgl_test_ne10_prepare_case(rotate_case, context);
    if (result == 0) {
        test_case.operation = rotate_case->name;
        test_case.backend = "ne10-c-bilinear";
        test_case.workers = 1U;
        test_case.width = (sgl_int32_t)context->width;
        test_case.height = (sgl_int32_t)context->height;
        test_case.pixels = (uint64_t)context->width *
            (uint64_t)context->height;
        result = (int)sgl_test_benchmark_measure(
            sgl_test_ne10_rotate_operation,
            context,
            SGL_TEST_WARMUP_COUNT,
            SGL_TEST_REPEAT_COUNT,
            &stats,
            samples,
            SGL_TEST_ARRAY_SIZE(samples));
    }
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(csv, index, &test_case, &stats);
        result = sgl_test_ne10_store_output(
            index, rotate_case, context);
    }

    return result;
}

static int sgl_test_run_ne10_rotate_cases(
    FILE *csv,
    size_t *case_index,
    sgl_test_ne10_rotate_context_t *context)
{
    static const sgl_test_ne10_rotate_case_t rotate_cases[] = {
        {"rotate-030", 30},
        {"rotate-045", 45},
        {"rotate-060", 60},
        {"rotate-090", 90},
        {"rotate-120", 120},
        {"rotate-180", 180},
        {"rotate-240", 240},
        {"rotate-270", 270},
        {"rotate-360", 360}
    };
    sgl_size_t rotate_index = 0U;
    int result = 0;

    while ((result == 0) &&
           (rotate_index < SGL_TEST_ARRAY_SIZE(rotate_cases))) {
        result = sgl_test_run_ne10_rotate_case(
            csv,
            *case_index,
            &rotate_cases[rotate_index],
            context);
        ++(*case_index);
        ++rotate_index;
    }

    return result;
}

int sgl_test_run_ne10_rotate_benchmark(
    FILE *csv,
    size_t *case_index,
    const sgl_test_png_t *png)
{
    sgl_test_ne10_rotate_context_t context;
    int result = 0;

    context.png = png;
    context.dst = SGL_NULL;
    context.capacity = 0U;
    context.angle = 0;
    context.width = 0U;
    context.height = 0U;
    if ((csv == SGL_NULL) ||
        (case_index == SGL_NULL) ||
        (png == SGL_NULL) ||
        (png->channels != (sgl_int32_t)SGL_TEST_NE10_RGBA_BYTES)) {
        result = 1;
    }
    if (result == 0) {
        context.capacity = sgl_test_ne10_rotate_capacity(png);
        context.dst = sgl_memory_as_uint8(
            sgl_calloc(context.capacity, sizeof(sgl_uint8_t)));
        if (context.dst == SGL_NULL) {
            result = 1;
        }
    }
    if (result == 0) {
        (void)printf(
            "NE10 comparison: RGBA bilinear C, 1 thread; "
            "upstream AArch64 has no rotate NEON implementation.\n");
        result = sgl_test_run_ne10_rotate_cases(
            csv, case_index, &context);
    }
    if (context.dst != SGL_NULL) {
        sgl_free(context.dst);
    }

    return result;
}
