/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <cairo.h>

#include <stdint.h>
#include <stdio.h>

#include <sgl-core.h>
#include <sgl_memory_cast.h>

#include "benchmark.h"
#include "cairo_benchmark.h"

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
#define SGL_TEST_CAIRO_ARGB32_BYTES    (4U)

typedef struct {
    const char *name;
    sgl_float64_t angle;
    double sin_angle;
    double cos_angle;
} sgl_test_cairo_rotate_case_t;

typedef struct {
    cairo_surface_t *dst_surface;
    cairo_t *cairo;
} sgl_test_cairo_rotate_context_t;

static int32_t sgl_test_cairo_rotate_operation(void *context)
{
    sgl_test_cairo_rotate_context_t *rotate;
    int32_t result = 0;

    /* cppcheck-suppress misra-c2012-11.5 */
    rotate = (sgl_test_cairo_rotate_context_t *)context;
    cairo_paint(rotate->cairo);
    cairo_surface_flush(rotate->dst_surface);
    if ((cairo_status(rotate->cairo) != CAIRO_STATUS_SUCCESS) ||
        (cairo_surface_status(rotate->dst_surface) !=
         CAIRO_STATUS_SUCCESS)) {
        result = 1;
    }

    return result;
}

static sgl_size_t sgl_test_cairo_rotate_capacity(
    const sgl_test_png_t *png)
{
    sgl_size_t side;
    sgl_size_t pixels = 0U;
    sgl_size_t result = 0U;

    side = (sgl_size_t)(sgl_uint32_t)png->width +
        (sgl_size_t)(sgl_uint32_t)png->height + 1U;
    if (side <= (SGL_SIZE_MAX / side)) {
        pixels = side * side;
        if (pixels <= (SGL_SIZE_MAX / SGL_TEST_CAIRO_ARGB32_BYTES)) {
            result = pixels * SGL_TEST_CAIRO_ARGB32_BYTES;
        }
    }

    return result;
}

static int sgl_test_cairo_rotate_prepare_context(
    cairo_surface_t *src_surface,
    sgl_uint8_t *dst,
    sgl_int32_t dst_width,
    sgl_int32_t dst_height,
    const sgl_test_png_t *png,
    const sgl_test_cairo_rotate_case_t *rotate_case,
    sgl_test_cairo_rotate_context_t *context)
{
    cairo_matrix_t matrix;
    cairo_pattern_t *pattern;
    double offset_x;
    double offset_y;
    int result = 0;

    context->dst_surface = cairo_image_surface_create_for_data(
        dst,
        CAIRO_FORMAT_ARGB32,
        dst_width,
        dst_height,
        dst_width * (sgl_int32_t)SGL_TEST_CAIRO_ARGB32_BYTES);
    context->cairo = cairo_create(context->dst_surface);
    if ((cairo_surface_status(context->dst_surface) !=
         CAIRO_STATUS_SUCCESS) ||
        (cairo_status(context->cairo) != CAIRO_STATUS_SUCCESS)) {
        result = 1;
    }
    if (result == 0) {
        cairo_set_operator(context->cairo, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(context->cairo, src_surface, 0.0, 0.0);
        pattern = cairo_get_source(context->cairo);

        /*
         * Cairo's pattern matrix maps destination pixel centers into source
         * coordinates.  The offsets rotate around both image centers:
         * src_x = cos(dst_x - dst_cx) + sin(dst_y - dst_cy) + src_cx.
         * Half-pixel terms are folded into width / 2 and height / 2 so an
         * identity matrix maps every destination pixel to the same source
         * pixel without a half-pixel translation.
         */
        offset_x = ((double)png->width * 0.5) -
            (rotate_case->cos_angle * (double)dst_width * 0.5) -
            (rotate_case->sin_angle * (double)dst_height * 0.5);
        offset_y = ((double)png->height * 0.5) +
            (rotate_case->sin_angle * (double)dst_width * 0.5) -
            (rotate_case->cos_angle * (double)dst_height * 0.5);
        cairo_matrix_init(
            &matrix,
            rotate_case->cos_angle,
            -rotate_case->sin_angle,
            rotate_case->sin_angle,
            rotate_case->cos_angle,
            offset_x,
            offset_y);
        cairo_pattern_set_matrix(pattern, &matrix);
        cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_NONE);
        if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
            result = 1;
        }
    }

    return result;
}

static void sgl_test_cairo_rotate_release_context(
    sgl_test_cairo_rotate_context_t *context)
{
    if (context->cairo != SGL_NULL) {
        cairo_destroy(context->cairo);
    }
    if (context->dst_surface != SGL_NULL) {
        cairo_surface_destroy(context->dst_surface);
    }
    context->cairo = SGL_NULL;
    context->dst_surface = SGL_NULL;
}

static int sgl_test_cairo_rotate_store_output(
    size_t index,
    const sgl_test_cairo_rotate_case_t *rotate_case,
    sgl_uint8_t *dst,
    sgl_int32_t dst_width,
    sgl_int32_t dst_height)
{
    sgl_test_png_t output;
    char path[FILENAME_MAX];
    int path_length;
    int result = 0;

    path_length = snprintf(
        path,
        sizeof(path),
        "%s/%03zu_rotate_cairo-nearest_1thread_%s_%dx%d.png",
        SGL_TEST_OUTPUT_DIR,
        index,
        rotate_case->name,
        dst_width,
        dst_height);
    if ((path_length < 0) || ((size_t)path_length >= sizeof(path))) {
        result = 1;
    }
    else {
        output.data = dst;
        output.width = dst_width;
        output.height = dst_height;
        output.channels = (sgl_int32_t)SGL_TEST_CAIRO_ARGB32_BYTES;
        if (sgl_test_save_png(&output, path) != 0) {
            result = 1;
        }
        else {
            (void)printf("  output[%03zu]: %s\n", index, path);
        }
    }

    return result;
}

static int sgl_test_run_cairo_rotate_case(
    FILE *csv,
    size_t index,
    cairo_surface_t *src_surface,
    const sgl_test_png_t *png,
    const sgl_test_cairo_rotate_case_t *rotate_case,
    sgl_uint8_t *dst,
    sgl_size_t capacity)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    sgl_test_cairo_rotate_context_t context;
    sgl_int32_t dst_width = 0;
    sgl_int32_t dst_height = 0;
    sgl_size_t dst_size = 0U;
    int result;

    context.dst_surface = SGL_NULL;
    context.cairo = SGL_NULL;
    result = (int)(sgl_get_rotated_dimensions(
        png->width,
        png->height,
        rotate_case->angle,
        &dst_width,
        &dst_height) != SGL_SUCCESS);
    if (result == 0) {
        dst_size = (sgl_size_t)(sgl_uint32_t)dst_width *
            (sgl_size_t)(sgl_uint32_t)dst_height *
            SGL_TEST_CAIRO_ARGB32_BYTES;
        if (dst_size > capacity) {
            result = 1;
        }
    }
    if (result == 0) {
        (void)sgl_memset(dst, 0, dst_size);
        result = sgl_test_cairo_rotate_prepare_context(
            src_surface,
            dst,
            dst_width,
            dst_height,
            png,
            rotate_case,
            &context);
    }
    if (result == 0) {
        test_case.operation = rotate_case->name;
        test_case.backend = "cairo-nearest";
        test_case.workers = 1U;
        test_case.width = dst_width;
        test_case.height = dst_height;
        test_case.pixels = (uint64_t)(sgl_uint32_t)dst_width *
            (uint64_t)(sgl_uint32_t)dst_height;
        result = (int)sgl_test_benchmark_measure(
            sgl_test_cairo_rotate_operation,
            &context,
            SGL_TEST_WARMUP_COUNT,
            SGL_TEST_REPEAT_COUNT,
            &stats,
            samples,
            SGL_TEST_ARRAY_SIZE(samples));
    }
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(csv, index, &test_case, &stats);
        result = sgl_test_cairo_rotate_store_output(
            index, rotate_case, dst, dst_width, dst_height);
    }
    sgl_test_cairo_rotate_release_context(&context);

    return result;
}

static int sgl_test_run_cairo_rotate_cases(
    FILE *csv,
    size_t *case_index,
    cairo_surface_t *src_surface,
    const sgl_test_png_t *png,
    sgl_uint8_t *dst,
    sgl_size_t capacity)
{
    static const sgl_test_cairo_rotate_case_t rotate_cases[] = {
        {"rotate-030", 30.0, 0.5, 0.8660254037844386},
        {"rotate-045", 45.0, 0.7071067811865476, 0.7071067811865476},
        {"rotate-060", 60.0, 0.8660254037844386, 0.5},
        {"rotate-090", 90.0, 1.0, 0.0},
        {"rotate-120", 120.0, 0.8660254037844386, -0.5},
        {"rotate-180", 180.0, 0.0, -1.0},
        {"rotate-240", 240.0, -0.8660254037844386, -0.5},
        {"rotate-270", 270.0, -1.0, 0.0},
        {"rotate-360", 360.0, 0.0, 1.0}
    };
    sgl_size_t rotate_index = 0U;
    int result = 0;

    while ((result == 0) &&
           (rotate_index < SGL_TEST_ARRAY_SIZE(rotate_cases))) {
        result = sgl_test_run_cairo_rotate_case(
            csv,
            *case_index,
            src_surface,
            png,
            &rotate_cases[rotate_index],
            dst,
            capacity);
        ++(*case_index);
        ++rotate_index;
    }

    return result;
}

int sgl_test_run_cairo_rotate_benchmark(
    FILE *csv,
    size_t *case_index,
    const sgl_test_png_t *png)
{
    cairo_surface_t *src_surface = SGL_NULL;
    sgl_uint8_t *dst = SGL_NULL;
    sgl_size_t capacity = 0U;
    int result = 0;

    if ((csv == SGL_NULL) || (case_index == SGL_NULL) ||
        (png == SGL_NULL) ||
        (png->channels != (sgl_int32_t)SGL_TEST_CAIRO_ARGB32_BYTES)) {
        result = 1;
    }
    if (result == 0) {
        capacity = sgl_test_cairo_rotate_capacity(png);
        dst = sgl_memory_as_uint8(sgl_malloc(capacity));
        if ((capacity == 0U) || (dst == SGL_NULL)) {
            result = 1;
        }
    }
    if (result == 0) {
        src_surface = cairo_image_surface_create_for_data(
            png->data,
            CAIRO_FORMAT_ARGB32,
            png->width,
            png->height,
            png->width * (sgl_int32_t)SGL_TEST_CAIRO_ARGB32_BYTES);
        if (cairo_surface_status(src_surface) != CAIRO_STATUS_SUCCESS) {
            result = 1;
        }
    }
    if (result == 0) {
        cairo_surface_mark_dirty(src_surface);
        (void)printf(
            "Cairo comparison: ARGB32 nearest-neighbor, 1 thread.\n");
        result = sgl_test_run_cairo_rotate_cases(
            csv, case_index, src_surface, png, dst, capacity);
    }
    if (src_surface != SGL_NULL) {
        cairo_surface_destroy(src_surface);
    }
    if (dst != SGL_NULL) {
        sgl_free(dst);
    }

    return result;
}
