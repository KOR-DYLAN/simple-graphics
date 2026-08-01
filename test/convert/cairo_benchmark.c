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

#include "benchmark.h"
#include "cairo_benchmark.h"

#ifndef SGL_TEST_REPEAT_COUNT
#define SGL_TEST_REPEAT_COUNT             (10U)
#endif
#ifndef SGL_TEST_WARMUP_COUNT
#define SGL_TEST_WARMUP_COUNT             (3U)
#endif
#ifndef SGL_TEST_CONVERT_OUTPUT_DIR
#define SGL_TEST_CONVERT_OUTPUT_DIR       "convert-output"
#endif

#define SGL_TEST_ARRAY_SIZE(a) \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_CAIRO_ARGB32_BYTES       (4U)
#define SGL_TEST_CAIRO_COMMAND_PATH       \
    SGL_TEST_CONVERT_OUTPUT_DIR "/cairo-ffplay-commands.txt"

typedef struct {
    const char *name;
    const char *ffplay_format;
    sgl_fourcc_t fourcc;
} sgl_test_cairo_convert_case_t;

typedef struct {
    cairo_surface_t *dst_surface;
    cairo_t *cairo;
} sgl_test_cairo_convert_context_t;

static int32_t sgl_test_cairo_convert_operation(void *context)
{
    sgl_test_cairo_convert_context_t *convert;
    int32_t result = 0;

    /* cppcheck-suppress misra-c2012-11.5 */
    convert = (sgl_test_cairo_convert_context_t *)context;
    cairo_paint(convert->cairo);
    cairo_surface_flush(convert->dst_surface);
    if ((cairo_status(convert->cairo) != CAIRO_STATUS_SUCCESS) ||
        (cairo_surface_status(convert->dst_surface) !=
         CAIRO_STATUS_SUCCESS)) {
        result = 1;
    }

    return result;
}

static int sgl_test_cairo_convert_compare(
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

static int sgl_test_cairo_convert_prepare_context(
    cairo_surface_t *src_surface,
    cairo_surface_t *dst_surface,
    sgl_test_cairo_convert_context_t *context)
{
    cairo_pattern_t *pattern;
    int result = 0;

    context->dst_surface = dst_surface;
    context->cairo = cairo_create(dst_surface);
    if (cairo_status(context->cairo) != CAIRO_STATUS_SUCCESS) {
        result = 1;
    }
    if (result == 0) {
        cairo_set_operator(context->cairo, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(context->cairo, src_surface, 0.0, 0.0);
        pattern = cairo_get_source(context->cairo);
        cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
        if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_cairo_convert_write_output(
    FILE *commands,
    size_t index,
    const sgl_test_cairo_convert_case_t *convert_case,
    const sgl_test_png_t *image,
    const sgl_uint8_t *dst,
    sgl_size_t bytes)
{
    char path[FILENAME_MAX];
    char filename[FILENAME_MAX];
    FILE *raw = SGL_NULL;
    int path_length;
    int filename_length;
    int result = 0;

    filename_length = snprintf(
        filename,
        sizeof(filename),
        "%04zu_convert_cairo-copy_rgb2rgb_%s-to-%s_1thread_%dx%d.raw",
        index,
        convert_case->name,
        convert_case->name,
        image->width,
        image->height);
    path_length = snprintf(
        path,
        sizeof(path),
        "%s/%s",
        SGL_TEST_CONVERT_OUTPUT_DIR,
        filename);
    if ((filename_length < 0) ||
        ((size_t)filename_length >= sizeof(filename)) ||
        (path_length < 0) || ((size_t)path_length >= sizeof(path))) {
        result = 1;
    }
    if (result == 0) {
        raw = fopen(path, "wb");
        if (raw == SGL_NULL) {
            result = 1;
        }
    }
    if (result == 0) {
        if (fwrite(dst, 1U, bytes, raw) != bytes) {
            result = 1;
        }
    }
    if ((raw != SGL_NULL) && (fclose(raw) != 0)) {
        result = 1;
    }
    if (result == 0) {
        if (fprintf(
                commands,
                "ffplay -loop 0 -f rawvideo -pixel_format %s "
                "-video_size %dx%d -framerate 1 -i %s\n",
                convert_case->ffplay_format,
                image->width,
                image->height,
                filename) < 0) {
            result = 1;
        }
        else {
            (void)printf("  output[%04zu]: %s\n", index, path);
        }
    }

    return result;
}

static int sgl_test_run_cairo_convert_case(
    FILE *csv,
    FILE *commands,
    size_t index,
    const sgl_test_cairo_convert_case_t *convert_case,
    const sgl_test_png_t *image,
    sgl_fourcc_t canonical_format,
    sgl_uint8_t *src,
    sgl_uint8_t *dst)
{
    uint64_t samples[SGL_TEST_REPEAT_COUNT];
    sgl_test_benchmark_stats_t stats;
    sgl_test_benchmark_case_t test_case;
    sgl_test_cairo_convert_context_t context;
    cairo_surface_t *src_surface = SGL_NULL;
    cairo_surface_t *dst_surface = SGL_NULL;
    char operation[32];
    sgl_size_t bytes;
    int operation_length;
    int result;

    context.dst_surface = SGL_NULL;
    context.cairo = SGL_NULL;
    bytes = (sgl_size_t)(sgl_uint32_t)image->width *
        (sgl_size_t)(sgl_uint32_t)image->height *
        SGL_TEST_CAIRO_ARGB32_BYTES;
    result = (int)(sgl_generic_rgb2rgb(
        SGL_NULL,
        image->data,
        canonical_format,
        src,
        convert_case->fourcc,
        image->width,
        image->height) != SGL_SUCCESS);
    operation_length = snprintf(
        operation,
        sizeof(operation),
        "%s-to-%s",
        convert_case->name,
        convert_case->name);
    if ((operation_length < 0) ||
        ((size_t)operation_length >= sizeof(operation))) {
        result = 1;
    }
    if (result == 0) {
        src_surface = cairo_image_surface_create_for_data(
            src,
            CAIRO_FORMAT_ARGB32,
            image->width,
            image->height,
            image->width * (sgl_int32_t)SGL_TEST_CAIRO_ARGB32_BYTES);
        dst_surface = cairo_image_surface_create_for_data(
            dst,
            CAIRO_FORMAT_ARGB32,
            image->width,
            image->height,
            image->width * (sgl_int32_t)SGL_TEST_CAIRO_ARGB32_BYTES);
        if ((cairo_surface_status(src_surface) != CAIRO_STATUS_SUCCESS) ||
            (cairo_surface_status(dst_surface) != CAIRO_STATUS_SUCCESS)) {
            result = 1;
        }
    }
    if (result == 0) {
        cairo_surface_mark_dirty(src_surface);
        result = sgl_test_cairo_convert_prepare_context(
            src_surface, dst_surface, &context);
    }
    if (result == 0) {
        test_case.operation = operation;
        test_case.backend = "cairo-copy";
        test_case.workers = 1U;
        test_case.width = image->width;
        test_case.height = image->height;
        test_case.pixels = (uint64_t)(sgl_uint32_t)image->width *
            (uint64_t)(sgl_uint32_t)image->height;
        result = (int)sgl_test_benchmark_measure(
            sgl_test_cairo_convert_operation,
            &context,
            SGL_TEST_WARMUP_COUNT,
            SGL_TEST_REPEAT_COUNT,
            &stats,
            samples,
            SGL_TEST_ARRAY_SIZE(samples));
    }
    if ((result == 0) &&
        (sgl_test_cairo_convert_compare(src, dst, bytes) != 0)) {
        result = 1;
    }
    if (result == 0) {
        sgl_test_benchmark_print_result(index, &test_case, &stats);
        sgl_test_benchmark_write_csv_result(csv, index, &test_case, &stats);
        result = sgl_test_cairo_convert_write_output(
            commands, index, convert_case, image, dst, bytes);
    }
    if (context.cairo != SGL_NULL) {
        cairo_destroy(context.cairo);
    }
    if (dst_surface != SGL_NULL) {
        cairo_surface_destroy(dst_surface);
    }
    if (src_surface != SGL_NULL) {
        cairo_surface_destroy(src_surface);
    }

    return result;
}

int sgl_test_run_cairo_convert_benchmark(
    FILE *csv,
    size_t *case_index,
    const sgl_test_png_t *image,
    sgl_fourcc_t canonical_format,
    sgl_uint8_t *src,
    sgl_uint8_t *dst)
{
    static const sgl_test_cairo_convert_case_t convert_cases[] = {
        {"rgba", "rgba", SGL_FOURCC_RGBA},
        {"argb", "argb", SGL_FOURCC_ARGB},
        {"bgra", "bgra", SGL_FOURCC_BGRA},
        {"abgr", "abgr", SGL_FOURCC_ABGR}
    };
    FILE *commands = SGL_NULL;
    sgl_size_t convert_index = 0U;
    int result = 0;

    if ((csv == SGL_NULL) || (case_index == SGL_NULL) ||
        (image == SGL_NULL) || (src == SGL_NULL) || (dst == SGL_NULL)) {
        result = 1;
    }
    if (result == 0) {
        commands = fopen(SGL_TEST_CAIRO_COMMAND_PATH, "w");
        if (commands == SGL_NULL) {
            result = 1;
        }
    }
    if (result == 0) {
        (void)printf(
            "Cairo comparison: 32-bit packed identity SOURCE copy, "
            "1 thread.\n");
        while ((result == 0) &&
               (convert_index < SGL_TEST_ARRAY_SIZE(convert_cases))) {
            result = sgl_test_run_cairo_convert_case(
                csv,
                commands,
                *case_index,
                &convert_cases[convert_index],
                image,
                canonical_format,
                src,
                dst);
            ++(*case_index);
            ++convert_index;
        }
    }
    if ((commands != SGL_NULL) && (fclose(commands) != 0)) {
        result = 1;
    }

    return result;
}
