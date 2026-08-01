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
#define SGL_TEST_OUTPUT_DIR            "crop-output"
#endif

#define SGL_TEST_ARRAY_SIZE(a) \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_CAIRO_ARGB32_BYTES    (4)

typedef struct {
    const char *name;
    sgl_int32_t x;
    sgl_int32_t y;
    sgl_int32_t width;
    sgl_int32_t height;
} sgl_test_cairo_crop_case_t;

typedef struct {
    cairo_surface_t *dst_surface;
    cairo_t *cairo;
} sgl_test_cairo_crop_context_t;

static int32_t sgl_test_cairo_crop_operation(void *context)
{
    sgl_test_cairo_crop_context_t *crop;
    int32_t result = 0;

    /* cppcheck-suppress misra-c2012-11.5 */
    crop = (sgl_test_cairo_crop_context_t *)context;
    cairo_paint(crop->cairo);
    cairo_surface_flush(crop->dst_surface);
    if ((cairo_status(crop->cairo) != CAIRO_STATUS_SUCCESS) ||
        (cairo_surface_status(crop->dst_surface) != CAIRO_STATUS_SUCCESS)) {
        result = 1;
    }

    return result;
}

static void sgl_test_cairo_initialize_crop_cases(
    const sgl_test_png_t *png,
    sgl_test_cairo_crop_case_t *cases)
{
    sgl_int32_t half_width;
    sgl_int32_t half_height;
    sgl_int32_t quarter_width;
    sgl_int32_t quarter_height;

    half_width = png->width >> 1U;
    half_height = png->height >> 1U;
    quarter_width = png->width >> 2U;
    quarter_height = png->height >> 2U;
    cases[0] = (sgl_test_cairo_crop_case_t){
        "full-frame", 0, 0, png->width, png->height};
    cases[1] = (sgl_test_cairo_crop_case_t){
        "top-left-quarter", 0, 0, half_width, half_height};
    cases[2] = (sgl_test_cairo_crop_case_t){
        "top-right-quarter", png->width - half_width, 0,
        half_width, half_height};
    cases[3] = (sgl_test_cairo_crop_case_t){
        "center-half", (png->width - half_width) >> 1U,
        (png->height - half_height) >> 1U, half_width, half_height};
    cases[4] = (sgl_test_cairo_crop_case_t){
        "bottom-left-quarter", 0, png->height - half_height,
        half_width, half_height};
    cases[5] = (sgl_test_cairo_crop_case_t){
        "bottom-right-quarter", png->width - half_width,
        png->height - half_height, half_width, half_height};
    cases[6] = (sgl_test_cairo_crop_case_t){
        "center-wide", png->width >> 3U,
        (png->height - quarter_height) >> 1U,
        png->width - (png->width >> 2U), quarter_height};
    cases[7] = (sgl_test_cairo_crop_case_t){
        "center-tall", (png->width - quarter_width) >> 1U,
        png->height >> 3U, quarter_width,
        png->height - (png->height >> 2U)};
}

static int sgl_test_cairo_crop_compare(
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

static int sgl_test_cairo_crop_store_output(
    size_t index,
    const sgl_test_cairo_crop_case_t *crop_case,
    sgl_uint8_t *dst)
{
    sgl_test_png_t output;
    char path[FILENAME_MAX];
    int path_length;
    int result = 0;

    path_length = snprintf(
        path,
        sizeof(path),
        "%s/%03zu_crop_cairo_1thread_%s_x%d_y%d_%dx%d.png",
        SGL_TEST_OUTPUT_DIR,
        index,
        crop_case->name,
        crop_case->x,
        crop_case->y,
        crop_case->width,
        crop_case->height);
    if ((path_length < 0) || ((size_t)path_length >= sizeof(path))) {
        result = 1;
    }
    else {
        output.data = dst;
        output.width = crop_case->width;
        output.height = crop_case->height;
        output.channels = SGL_TEST_CAIRO_ARGB32_BYTES;
        if (sgl_test_save_png(&output, path) != 0) {
            result = 1;
        }
        else {
            (void)printf("  output[%03zu]: %s\n", index, path);
        }
    }

    return result;
}

static int sgl_test_cairo_crop_prepare_context(
    cairo_surface_t *src_surface,
    sgl_uint8_t *dst,
    const sgl_test_cairo_crop_case_t *crop_case,
    sgl_test_cairo_crop_context_t *context)
{
    cairo_pattern_t *pattern;
    int result = 0;

    context->dst_surface = cairo_image_surface_create_for_data(
        dst,
        CAIRO_FORMAT_ARGB32,
        crop_case->width,
        crop_case->height,
        crop_case->width * SGL_TEST_CAIRO_ARGB32_BYTES);
    context->cairo = cairo_create(context->dst_surface);
    if ((cairo_surface_status(context->dst_surface) != CAIRO_STATUS_SUCCESS) ||
        (cairo_status(context->cairo) != CAIRO_STATUS_SUCCESS)) {
        result = 1;
    }
    if (result == 0) {
        cairo_set_operator(context->cairo, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(
            context->cairo,
            src_surface,
            -(double)crop_case->x,
            -(double)crop_case->y);
        pattern = cairo_get_source(context->cairo);
        cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
        if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
            result = 1;
        }
    }

    return result;
}

static void sgl_test_cairo_crop_release_context(
    sgl_test_cairo_crop_context_t *context)
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

static int sgl_test_run_cairo_crop_case(
    FILE *csv,
    size_t index,
    cairo_surface_t *src_surface,
    const sgl_test_png_t *png,
    const sgl_test_cairo_crop_case_t *crop_case,
    sgl_uint8_t *dst,
    sgl_uint8_t *reference)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    sgl_test_cairo_crop_context_t context;
    sgl_size_t bytes;
    int result;

    context.dst_surface = SGL_NULL;
    context.cairo = SGL_NULL;
    bytes = (sgl_size_t)(sgl_uint32_t)crop_case->width *
        (sgl_size_t)(sgl_uint32_t)crop_case->height *
        (sgl_size_t)SGL_TEST_CAIRO_ARGB32_BYTES;
    result = (int)(sgl_generic_crop(
        SGL_NULL,
        png->data,
        png->width,
        png->height,
        reference,
        crop_case->x,
        crop_case->y,
        crop_case->width,
        crop_case->height,
        SGL_TEST_CAIRO_ARGB32_BYTES) != SGL_SUCCESS);
    if (result == 0) {
        result = sgl_test_cairo_crop_prepare_context(
            src_surface, dst, crop_case, &context);
    }
    if (result == 0) {
        test_case.operation = crop_case->name;
        test_case.backend = "cairo";
        test_case.workers = 1U;
        test_case.width = crop_case->width;
        test_case.height = crop_case->height;
        test_case.pixels = (uint64_t)(sgl_uint32_t)crop_case->width *
            (uint64_t)(sgl_uint32_t)crop_case->height;
        result = (int)sgl_test_benchmark_measure(
            sgl_test_cairo_crop_operation,
            &context,
            SGL_TEST_WARMUP_COUNT,
            SGL_TEST_REPEAT_COUNT,
            &stats,
            samples,
            SGL_TEST_ARRAY_SIZE(samples));
    }
    if ((result == 0) &&
        (sgl_test_cairo_crop_compare(reference, dst, bytes) != 0)) {
        result = 1;
    }
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(csv, index, &test_case, &stats);
        result = sgl_test_cairo_crop_store_output(index, crop_case, dst);
    }
    sgl_test_cairo_crop_release_context(&context);

    return result;
}

int sgl_test_run_cairo_crop_benchmark(
    FILE *csv,
    size_t *case_index,
    const sgl_test_png_t *png)
{
    sgl_test_cairo_crop_case_t crop_cases[8];
    cairo_surface_t *src_surface = SGL_NULL;
    sgl_uint8_t *dst = SGL_NULL;
    sgl_uint8_t *reference = SGL_NULL;
    sgl_size_t capacity = 0U;
    sgl_size_t crop_index = 0U;
    int result = 0;

    if ((csv == SGL_NULL) || (case_index == SGL_NULL) ||
        (png == SGL_NULL) ||
        (png->channels != SGL_TEST_CAIRO_ARGB32_BYTES)) {
        result = 1;
    }
    if (result == 0) {
        capacity = (sgl_size_t)(sgl_uint32_t)png->width *
            (sgl_size_t)(sgl_uint32_t)png->height *
            (sgl_size_t)SGL_TEST_CAIRO_ARGB32_BYTES;
        dst = sgl_memory_as_uint8(sgl_malloc(capacity));
        reference = sgl_memory_as_uint8(sgl_malloc(capacity));
        if ((dst == SGL_NULL) || (reference == SGL_NULL)) {
            result = 1;
        }
    }
    if (result == 0) {
        src_surface = cairo_image_surface_create_for_data(
            png->data,
            CAIRO_FORMAT_ARGB32,
            png->width,
            png->height,
            png->width * SGL_TEST_CAIRO_ARGB32_BYTES);
        if (cairo_surface_status(src_surface) != CAIRO_STATUS_SUCCESS) {
            result = 1;
        }
    }
    if (result == 0) {
        cairo_surface_mark_dirty(src_surface);
        sgl_test_cairo_initialize_crop_cases(png, crop_cases);
        while ((result == 0) &&
               (crop_index < SGL_TEST_ARRAY_SIZE(crop_cases))) {
            result = sgl_test_run_cairo_crop_case(
                csv,
                *case_index,
                src_surface,
                png,
                &crop_cases[crop_index],
                dst,
                reference);
            ++(*case_index);
            ++crop_index;
        }
    }
    if (src_surface != SGL_NULL) {
        cairo_surface_destroy(src_surface);
    }
    if (reference != SGL_NULL) {
        sgl_free(reference);
    }
    if (dst != SGL_NULL) {
        sgl_free(dst);
    }

    return result;
}
