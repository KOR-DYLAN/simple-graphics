/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sgl-core.h>
#include <sgl_memory_cast.h>

#include "raw_output.h"
#include "source.h"
#include "util.h"

#ifndef SGL_TEST_CONVERT_MATRIX_WIDTH
#define SGL_TEST_CONVERT_MATRIX_WIDTH     (1280)
#endif
#ifndef SGL_TEST_CONVERT_MATRIX_HEIGHT
#define SGL_TEST_CONVERT_MATRIX_HEIGHT    (720)
#endif

#define SGL_TEST_RAW_ARRAY_SIZE(a)        \
    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_RAW_MEMORY_POOL_SIZE      (96U * 1024U * 1024U)
#define SGL_TEST_RAW_WORKERS               (4U)
#define SGL_TEST_RAW_PATH_CAPACITY         (512U)
#define SGL_TEST_RAW_INDEX_DIGITS          (4U)
#define SGL_TEST_RAW_CONVERSION_WIDTH      (30U)
#define SGL_TEST_RAW_BACKEND_WIDTH         (7U)
#define SGL_TEST_RAW_THREAD_WIDTH          (2U)
#define SGL_TEST_RAW_ELAPSED_WIDTH         (11U)
#define SGL_TEST_RAW_COMMAND_PATH          \
    SGL_TEST_CONVERT_OUTPUT_DIR "/ffplay-commands.txt"
#define SGL_TEST_RAW_YUV_FILTER            \
    "scale=in_range=full:in_color_matrix=bt601,format=rgb24"

typedef sgl_result_t (*sgl_test_convert_runner_t)(
    sgl_threadpool_t *pool,
    const sgl_uint8_t *src,
    sgl_fourcc_t src_format,
    sgl_uint8_t *dst,
    sgl_fourcc_t dst_format,
    sgl_int32_t width,
    sgl_int32_t height);

typedef enum {
    SGL_TEST_RAW_RGB = 0,
    SGL_TEST_RAW_YUV = 1
} sgl_test_raw_family_t;

typedef enum {
    SGL_TEST_RAW_SIZE_4_BYTES,
    SGL_TEST_RAW_SIZE_3_BYTES,
    SGL_TEST_RAW_SIZE_YUV422,
    SGL_TEST_RAW_SIZE_YUV420
} sgl_test_raw_size_t;

typedef enum {
    SGL_TEST_RAW_BACKEND_GENERIC,
    SGL_TEST_RAW_BACKEND_SIMD
} sgl_test_raw_backend_id_t;

typedef struct {
    const char *name;
    const char *display_name;
    sgl_fourcc_t fourcc;
    sgl_test_raw_family_t family;
    sgl_test_raw_size_t size;
    const char *ffplay_format;
    const char *ffplay_filter;
} sgl_test_raw_format_t;

typedef struct {
    const char *name;
    sgl_test_convert_runner_t generic_runner;
    sgl_test_convert_runner_t simd_runner;
} sgl_test_raw_operation_t;

typedef struct {
    const char *name;
    sgl_test_raw_backend_id_t id;
} sgl_test_raw_backend_t;

typedef struct {
    sgl_size_t workers;
    sgl_threadpool_t *pool;
} sgl_test_raw_thread_t;

typedef struct {
    char value[SGL_TEST_RAW_PATH_CAPACITY];
    sgl_size_t length;
    sgl_size_t filename_offset;
    sgl_bool_t valid;
} sgl_test_raw_path_t;

typedef struct {
    sgl_test_png_t *png;
    sgl_threadpool_t *pool;
    sgl_uint8_t *canonical;
    sgl_uint8_t *src;
    sgl_uint8_t *reference;
    sgl_uint8_t *backend_reference;
    sgl_uint8_t *dst;
    sgl_fourcc_t canonical_format;
    sgl_int32_t width;
    sgl_int32_t height;
    sgl_size_t pixels;
    sgl_size_t capacity;
    int command_descriptor;
} sgl_test_raw_resources_t;

static const sgl_test_raw_format_t sgl_test_raw_formats[] = {
    {"rgba", "RGBA", SGL_FOURCC_RGBA, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "rgba", SGL_NULL},
    {"argb", "ARGB", SGL_FOURCC_ARGB, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "argb", SGL_NULL},
    {"bgra", "BGRA", SGL_FOURCC_BGRA, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "bgra", SGL_NULL},
    {"abgr", "ABGR", SGL_FOURCC_ABGR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "abgr", SGL_NULL},
    {"rgb", "RGB", SGL_FOURCC_RGB, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_3_BYTES, "rgb24", SGL_NULL},
    {"bgr", "BGR", SGL_FOURCC_BGR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_3_BYTES, "bgr24", SGL_NULL},
    {"rgba-planar", "RGBA planar", SGL_FOURCC_RGBA_PLANAR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "gbrap", "shuffleplanes=1:2:0:3"},
    {"argb-planar", "ARGB planar", SGL_FOURCC_ARGB_PLANAR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "gbrap", "shuffleplanes=2:3:1:0"},
    {"bgra-planar", "BGRA planar", SGL_FOURCC_BGRA_PLANAR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "gbrap", "shuffleplanes=1:0:2:3"},
    {"abgr-planar", "ABGR planar", SGL_FOURCC_ABGR_PLANAR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_4_BYTES, "gbrap", "shuffleplanes=2:1:3:0"},
    {"rgb-planar", "RGB planar", SGL_FOURCC_RGB_PLANAR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_3_BYTES, "gbrp", "shuffleplanes=1:2:0"},
    {"bgr-planar", "BGR planar", SGL_FOURCC_BGR_PLANAR, SGL_TEST_RAW_RGB,
     SGL_TEST_RAW_SIZE_3_BYTES, "gbrp", "shuffleplanes=1:0:2"},
    {"yuv444-packed", "YUV444 packed", SGL_FOURCC_YUV444_PACKED,
     SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_3_BYTES, SGL_NULL, SGL_NULL},
    {"yuv444-planar", "YUV444 planar", SGL_FOURCC_YUV444_PLANAR,
     SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_3_BYTES, "yuv444p", SGL_TEST_RAW_YUV_FILTER},
    {"yuyv", "YUYV", SGL_FOURCC_YUYV, SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV422, "yuyv422", SGL_TEST_RAW_YUV_FILTER},
    {"yvyu", "YVYU", SGL_FOURCC_YVYU, SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV422, "yvyu422", SGL_TEST_RAW_YUV_FILTER},
    {"uyvy", "UYVY", SGL_FOURCC_UYVY, SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV422, "uyvy422", SGL_TEST_RAW_YUV_FILTER},
    {"vyuy", "VYUY", SGL_FOURCC_VYUY, SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV422, "uyvy422",
     "swapuv," SGL_TEST_RAW_YUV_FILTER},
    {"yuv422-planar", "YUV422 planar", SGL_FOURCC_YUV422_PLANAR,
     SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV422, "yuv422p", SGL_TEST_RAW_YUV_FILTER},
    {"yuv420-planar", "YUV420 planar", SGL_FOURCC_YUV420_PLANAR,
     SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV420, "yuv420p", SGL_TEST_RAW_YUV_FILTER},
    {"nv12", "NV12", SGL_FOURCC_NV12, SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV420, "nv12", SGL_TEST_RAW_YUV_FILTER},
    {"nv21", "NV21", SGL_FOURCC_NV21, SGL_TEST_RAW_YUV,
     SGL_TEST_RAW_SIZE_YUV420, "nv21", SGL_TEST_RAW_YUV_FILTER}
};

static const sgl_test_raw_operation_t sgl_test_raw_operations[] = {
    {
        "rgb2rgb",
        sgl_generic_rgb2rgb,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_rgb2rgb
#else
        SGL_NULL
#endif
    },
    {
        "rgb2yuv",
        sgl_generic_rgb2yuv,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_rgb2yuv
#else
        SGL_NULL
#endif
    },
    {
        "yuv2rgb",
        sgl_generic_yuv2rgb,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_yuv2rgb
#else
        SGL_NULL
#endif
    },
    {
        "yuv2yuv",
        sgl_generic_yuv2yuv,
#if defined(SGL_CFG_HAS_NEON)
        sgl_simd_yuv2yuv
#else
        SGL_NULL
#endif
    }
};

static const sgl_test_raw_backend_t sgl_test_raw_backends[] = {
    {"generic", SGL_TEST_RAW_BACKEND_GENERIC},
#if defined(SGL_CFG_HAS_NEON)
    {"simd", SGL_TEST_RAW_BACKEND_SIMD}
#endif
};

static sgl_uint8_t sgl_test_raw_memory_pool[
    SGL_TEST_RAW_MEMORY_POOL_SIZE];

static int sgl_test_open_output(const char *path)
{
    int descriptor;

    /*
     * The bitwise OR combines independent POSIX access, creation, and
     * truncation flags. The permission flags produce mode 0644.
     */
    descriptor = open(
        path,
        O_WRONLY | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    return descriptor;
}

static sgl_size_t sgl_test_text_length(const char *text)
{
    sgl_size_t result = 0U;

    while (text[result] != '\0') {
        ++result;
    }

    return result;
}

static int sgl_test_write_data(
    int descriptor,
    const sgl_uint8_t *data,
    sgl_size_t size)
{
    sgl_size_t offset = 0U;
    int result = 0;

    while ((offset < size) && (result == 0)) {
        ssize_t written;

        written = write(descriptor, &data[offset], size - offset);
        if (written > 0) {
            offset += (sgl_size_t)written;
        }
        else {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_write_text(int descriptor, const char *text)
{
    int result;

    result = sgl_test_write_data(
        descriptor,
        (const sgl_uint8_t *)text,
        sgl_test_text_length(text));

    return result;
}

static int sgl_test_write_uint64_decimal(
    int descriptor,
    sgl_uint64_t value)
{
    char digits[21];
    sgl_size_t count = 0U;
    sgl_size_t index;
    int result;

    do {
        digits[count] = (char)('0' + (char)(value % 10ULL));
        value /= 10ULL;
        ++count;
    } while (value > 0ULL);

    index = 0U;
    /* Shifting right by one divides the digit count by two. */
    while (index < (count >> 1U)) {
        char temporary;

        temporary = digits[index];
        digits[index] = digits[count - index - 1U];
        digits[count - index - 1U] = temporary;
        ++index;
    }
    digits[count] = '\0';
    result = sgl_test_write_text(descriptor, digits);

    return result;
}

static int sgl_test_write_uint32_decimal(
    int descriptor,
    sgl_uint32_t value)
{
    int result;

    result = sgl_test_write_uint64_decimal(
        descriptor, (sgl_uint64_t)value);

    return result;
}

static int sgl_test_write_spaces(int descriptor, sgl_size_t count)
{
    static const char spaces[] =
        "                                ";
    int result = 0;

    if (count > (sizeof(spaces) - 1U)) {
        result = 1;
    }
    if ((result == 0) && (count > 0U)) {
        result = sgl_test_write_data(
            descriptor, (const sgl_uint8_t *)spaces, count);
    }

    return result;
}

static int sgl_test_write_text_left(
    int descriptor,
    const char *value,
    sgl_size_t width)
{
    sgl_size_t length;
    int result;

    length = sgl_test_text_length(value);
    result = sgl_test_write_text(descriptor, value);
    if ((result == 0) && (length < width)) {
        result = sgl_test_write_spaces(descriptor, width - length);
    }

    return result;
}

static sgl_size_t sgl_test_uint64_decimal_length(sgl_uint64_t value)
{
    sgl_size_t result = 1U;

    while (value >= 10ULL) {
        value /= 10ULL;
        ++result;
    }

    return result;
}

static int sgl_test_write_uint64_right(
    int descriptor,
    sgl_uint64_t value,
    sgl_size_t width)
{
    sgl_size_t length;
    int result = 0;

    length = sgl_test_uint64_decimal_length(value);
    if (length < width) {
        result = sgl_test_write_spaces(descriptor, width - length);
    }
    if (result == 0) {
        result = sgl_test_write_uint64_decimal(descriptor, value);
    }

    return result;
}

static int sgl_test_write_raw_file(
    const char *path,
    const sgl_uint8_t *data,
    sgl_size_t size)
{
    int descriptor;
    int result = 0;

    descriptor = sgl_test_open_output(path);
    if (descriptor < 0) {
        result = 1;
    }
    else {
        result = sgl_test_write_data(descriptor, data, size);
        if (close(descriptor) != 0) {
            result = 1;
        }
    }

    return result;
}

static void sgl_test_print_output_header(void)
{
    (void)sgl_test_write_text(
        STDOUT_FILENO,
        "\nConvert raw outputs\n"
        "case  conversion                      backend  th  elapsed(us)  output\n"
        "----  ------------------------------  -------  --  -----------  ------\n");
}

static void sgl_test_print_conversion(
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format)
{
    sgl_size_t length;

    length = sgl_test_text_length(src_format->display_name) +
        sgl_test_text_length(dst_format->display_name) + 4U;
    (void)sgl_test_write_text(STDOUT_FILENO, src_format->display_name);
    (void)sgl_test_write_text(STDOUT_FILENO, " -> ");
    (void)sgl_test_write_text(STDOUT_FILENO, dst_format->display_name);
    if (length < SGL_TEST_RAW_CONVERSION_WIDTH) {
        (void)sgl_test_write_spaces(
            STDOUT_FILENO, SGL_TEST_RAW_CONVERSION_WIDTH - length);
    }
}

static void sgl_test_print_output_case(
    sgl_size_t index,
    const sgl_test_raw_backend_t *backend,
    const sgl_test_raw_thread_t *thread,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    const sgl_test_raw_path_t *path,
    sgl_uint64_t elapsed_us)
{
    (void)sgl_test_write_uint64_right(
        STDOUT_FILENO, (sgl_uint64_t)index, SGL_TEST_RAW_INDEX_DIGITS);
    (void)sgl_test_write_text(STDOUT_FILENO, "  ");
    sgl_test_print_conversion(src_format, dst_format);
    (void)sgl_test_write_text(STDOUT_FILENO, "  ");
    (void)sgl_test_write_text_left(
        STDOUT_FILENO, backend->name, SGL_TEST_RAW_BACKEND_WIDTH);
    (void)sgl_test_write_text(STDOUT_FILENO, "  ");
    (void)sgl_test_write_uint64_right(
        STDOUT_FILENO,
        (sgl_uint64_t)thread->workers,
        SGL_TEST_RAW_THREAD_WIDTH);
    (void)sgl_test_write_text(STDOUT_FILENO, "  ");
    (void)sgl_test_write_uint64_right(
        STDOUT_FILENO, elapsed_us, SGL_TEST_RAW_ELAPSED_WIDTH);
    (void)sgl_test_write_text(STDOUT_FILENO, "  ");
    (void)sgl_test_write_text(STDOUT_FILENO, path->value);
    (void)sgl_test_write_text(STDOUT_FILENO, "\n");
}

static void sgl_test_raw_path_initialize(sgl_test_raw_path_t *path)
{
    path->value[0] = '\0';
    path->length = 0U;
    path->filename_offset = 0U;
    path->valid = SGL_TRUE;
}

static void sgl_test_raw_path_append_char(
    sgl_test_raw_path_t *path,
    char value)
{
    if ((path->valid == SGL_TRUE) &&
        ((path->length + 1U) < SGL_TEST_RAW_PATH_CAPACITY)) {
        path->value[path->length] = value;
        ++path->length;
        path->value[path->length] = '\0';
    }
    else {
        path->valid = SGL_FALSE;
    }
}

static void sgl_test_raw_path_append_text(
    sgl_test_raw_path_t *path,
    const char *text)
{
    sgl_size_t index = 0U;

    while (text[index] != '\0') {
        sgl_test_raw_path_append_char(path, text[index]);
        ++index;
    }
}

static void sgl_test_raw_path_append_uint32(
    sgl_test_raw_path_t *path,
    sgl_uint32_t value,
    sgl_size_t minimum_digits)
{
    char digits[11];
    sgl_size_t count = 0U;

    do {
        digits[count] = (char)('0' + (char)(value % 10U));
        value /= 10U;
        ++count;
    } while (value > 0U);
    while (count < minimum_digits) {
        digits[count] = '0';
        ++count;
    }
    while (count > 0U) {
        --count;
        sgl_test_raw_path_append_char(path, digits[count]);
    }
}

static sgl_test_raw_path_t sgl_test_build_output_path(
    sgl_size_t index,
    const sgl_test_raw_operation_t *operation,
    const sgl_test_raw_backend_t *backend,
    const sgl_test_raw_thread_t *thread,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    const sgl_test_raw_resources_t *resources)
{
    sgl_test_raw_path_t result;

    sgl_test_raw_path_initialize(&result);
    sgl_test_raw_path_append_text(&result, SGL_TEST_CONVERT_OUTPUT_DIR);
    sgl_test_raw_path_append_char(&result, '/');
    result.filename_offset = result.length;
    sgl_test_raw_path_append_uint32(
        &result, (sgl_uint32_t)index, SGL_TEST_RAW_INDEX_DIGITS);
    sgl_test_raw_path_append_text(&result, "_convert_");
    sgl_test_raw_path_append_text(&result, backend->name);
    sgl_test_raw_path_append_char(&result, '_');
    sgl_test_raw_path_append_text(&result, operation->name);
    sgl_test_raw_path_append_char(&result, '_');
    sgl_test_raw_path_append_text(&result, src_format->name);
    sgl_test_raw_path_append_text(&result, "-to-");
    sgl_test_raw_path_append_text(&result, dst_format->name);
    sgl_test_raw_path_append_char(&result, '_');
    sgl_test_raw_path_append_uint32(
        &result, (sgl_uint32_t)thread->workers, 1U);
    sgl_test_raw_path_append_text(&result, "thread_");
    sgl_test_raw_path_append_uint32(
        &result, (sgl_uint32_t)resources->width, 1U);
    sgl_test_raw_path_append_char(&result, 'x');
    sgl_test_raw_path_append_uint32(
        &result, (sgl_uint32_t)resources->height, 1U);
    sgl_test_raw_path_append_text(&result, ".raw");

    return result;
}

static int sgl_test_write_command_geometry(
    int descriptor,
    const sgl_test_raw_format_t *format,
    const sgl_test_raw_resources_t *resources)
{
    int result;

    result = sgl_test_write_text(
        descriptor, "ffplay -loop 0 -f rawvideo -pixel_format ");
    if (result == 0) {
        result = sgl_test_write_text(descriptor, format->ffplay_format);
    }
    if (result == 0) {
        result = sgl_test_write_text(descriptor, " -video_size ");
    }
    if (result == 0) {
        result = sgl_test_write_uint32_decimal(
            descriptor, (sgl_uint32_t)resources->width);
    }
    if (result == 0) {
        result = sgl_test_write_text(descriptor, "x");
    }
    if (result == 0) {
        result = sgl_test_write_uint32_decimal(
            descriptor, (sgl_uint32_t)resources->height);
    }
    if (result == 0) {
        result = sgl_test_write_text(descriptor, " -framerate 1");
    }

    return result;
}

static int sgl_test_write_command_filter(
    int descriptor,
    const sgl_test_raw_format_t *format)
{
    int result = 0;

    if (format->ffplay_filter != SGL_NULL) {
        result = sgl_test_write_text(descriptor, " -vf \"");
        if (result == 0) {
            result = sgl_test_write_text(
                descriptor, format->ffplay_filter);
        }
        if (result == 0) {
            result = sgl_test_write_text(descriptor, "\"");
        }
    }

    return result;
}

static int sgl_test_write_ffplay_command(
    int descriptor,
    const sgl_test_raw_format_t *format,
    const sgl_test_raw_resources_t *resources,
    const sgl_test_raw_path_t *path)
{
    int result = 0;

    if (format->ffplay_format != SGL_NULL) {
        result = sgl_test_write_command_geometry(
            descriptor, format, resources);
        if (result == 0) {
            result = sgl_test_write_command_filter(descriptor, format);
        }
        if (result == 0) {
            result = sgl_test_write_text(descriptor, " -i ");
        }
        if (result == 0) {
            result = sgl_test_write_text(
                descriptor, &path->value[path->filename_offset]);
        }
        if (result == 0) {
            result = sgl_test_write_text(descriptor, "\n");
        }
    }

    return result;
}

static sgl_size_t sgl_test_get_raw_size(
    const sgl_test_raw_format_t *format,
    const sgl_test_raw_resources_t *resources)
{
    sgl_size_t chroma_width;
    sgl_size_t chroma_height;
    sgl_size_t result;

    /* Add one then remove one bit to calculate ceil(dimension / 2). */
    chroma_width = (
        (sgl_size_t)(sgl_uint32_t)resources->width + 1U) >> 1U;
    chroma_height = (
        (sgl_size_t)(sgl_uint32_t)resources->height + 1U) >> 1U;
    switch (format->size) {
    case SGL_TEST_RAW_SIZE_4_BYTES:
        /* Left shift by two multiplies pixels by four bytes. */
        result = resources->pixels << 2U;
        break;
    case SGL_TEST_RAW_SIZE_3_BYTES:
        /* Two pixel units plus one unit equals three bytes per pixel. */
        result = (resources->pixels << 1U) + resources->pixels;
        break;
    case SGL_TEST_RAW_SIZE_YUV422:
        /* Each horizontal chroma pair stores Y0,U,Y1,V: four bytes. */
        result = (
            chroma_width * (sgl_size_t)(sgl_uint32_t)resources->height) << 2U;
        break;
    default:
        /* Add luma and the two ceil(width/2) x ceil(height/2) planes. */
        result = resources->pixels +
            ((chroma_width * chroma_height) << 1U);
        break;
    }

    return result;
}

static const sgl_test_raw_operation_t *sgl_test_get_operation(
    sgl_test_raw_family_t src_family,
    sgl_test_raw_family_t dst_family)
{
    const sgl_test_raw_operation_t *result;
    sgl_uint32_t mode;

    /*
     * The source family occupies bit 1 and the destination family bit 0:
     * 00 RGB->RGB, 01 RGB->YUV, 10 YUV->RGB, 11 YUV->YUV.
     */
    mode = ((sgl_uint32_t)src_family << 1U) |
        (sgl_uint32_t)dst_family;
    switch (mode) {
    case 0U:
        result = &sgl_test_raw_operations[0];
        break;
    case 1U:
        result = &sgl_test_raw_operations[1];
        break;
    case 2U:
        result = &sgl_test_raw_operations[2];
        break;
    default:
        result = &sgl_test_raw_operations[3];
        break;
    }

    return result;
}

static sgl_test_convert_runner_t sgl_test_get_runner(
    const sgl_test_raw_operation_t *operation,
    sgl_test_raw_backend_id_t backend)
{
    sgl_test_convert_runner_t result;

    switch (backend) {
    case SGL_TEST_RAW_BACKEND_GENERIC:
        result = operation->generic_runner;
        break;
    case SGL_TEST_RAW_BACKEND_SIMD:
        result = operation->simd_runner;
        break;
    default:
        result = SGL_NULL;
        break;
    }

    return result;
}

static int sgl_test_compare_output(
    const sgl_uint8_t *expected,
    const sgl_uint8_t *actual,
    sgl_size_t size)
{
    sgl_size_t index = 0U;
    int result = 0;

    while (index < size) {
        /*
         * XOR identifies differing byte bits; OR retains every mismatch while
         * preserving fixed loop control.
         */
        result |= (int)expected[index] ^ (int)actual[index];
        ++index;
    }

    return result;
}

static int sgl_test_prepare_source_format(
    const sgl_test_raw_format_t *format,
    sgl_test_raw_resources_t *resources)
{
    sgl_result_t convert_result;
    int result;

    switch (format->family) {
    case SGL_TEST_RAW_RGB:
        convert_result = sgl_generic_rgb2rgb(
            SGL_NULL,
            resources->canonical,
            resources->canonical_format,
            resources->src,
            format->fourcc,
            resources->width,
            resources->height);
        break;
    case SGL_TEST_RAW_YUV:
        convert_result = sgl_generic_rgb2yuv(
            SGL_NULL,
            resources->canonical,
            resources->canonical_format,
            resources->src,
            format->fourcc,
            resources->width,
            resources->height);
        break;
    default:
        convert_result = SGL_ERROR_NOT_SUPPORTED;
        break;
    }
    result = (int)(convert_result != SGL_SUCCESS);

    return result;
}

static int sgl_test_store_output_case(
    sgl_size_t index,
    const sgl_test_raw_operation_t *operation,
    const sgl_test_raw_backend_t *backend,
    const sgl_test_raw_thread_t *thread,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    const sgl_test_raw_resources_t *resources,
    const sgl_uint8_t *data,
    sgl_size_t output_size,
    sgl_uint64_t elapsed_us)
{
    sgl_test_raw_path_t path;
    int result = 0;

    path = sgl_test_build_output_path(
        index, operation, backend, thread,
        src_format, dst_format, resources);
    if (path.valid != SGL_TRUE) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_write_raw_file(
            path.value, data, output_size) != 0)) {
        result = 1;
    }
    if (result == 0) {
        sgl_test_print_output_case(
            index, backend, thread,
            src_format, dst_format, &path, elapsed_us);
    }
    if ((result == 0) &&
        (sgl_test_write_ffplay_command(
            resources->command_descriptor,
            dst_format,
            resources,
            &path) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_run_conversion(
    sgl_test_convert_runner_t runner,
    const sgl_test_raw_thread_t *thread,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    const sgl_test_raw_resources_t *resources,
    sgl_uint8_t *dst,
    sgl_uint64_t *elapsed_us)
{
    sgl_result_t convert_result;
    sgl_uint64_t start_us;
    int result;

    if ((runner == SGL_NULL) || (elapsed_us == SGL_NULL)) {
        result = 1;
    }
    else {
        start_us = sgl_test_get_timestamp_us(0ULL);
        convert_result = runner(
            thread->pool,
            resources->src,
            src_format->fourcc,
            dst,
            dst_format->fourcc,
            resources->width,
            resources->height);
        *elapsed_us = sgl_test_get_timestamp_us(start_us);
        result = (int)(convert_result != SGL_SUCCESS);
    }

    return result;
}

static int sgl_test_run_reference_case(
    sgl_size_t index,
    const sgl_test_raw_operation_t *operation,
    const sgl_test_raw_backend_t *backend,
    const sgl_test_raw_thread_t *thread,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    sgl_test_raw_resources_t *resources)
{
    sgl_test_convert_runner_t runner;
    sgl_size_t output_size;
    sgl_uint64_t elapsed_us = 0ULL;
    int conversion_result;
    int result = 0;

    runner = sgl_test_get_runner(operation, backend->id);
    output_size = sgl_test_get_raw_size(dst_format, resources);
    (void)sgl_memset(
        resources->backend_reference, 0, resources->capacity);
    conversion_result = sgl_test_run_conversion(
        runner, thread, src_format, dst_format, resources,
        resources->backend_reference, &elapsed_us);
    result = conversion_result;
    if ((conversion_result == 0) &&
        (backend->id == SGL_TEST_RAW_BACKEND_GENERIC)) {
        (void)sgl_memcpy(
            resources->reference,
            resources->backend_reference,
            output_size);
    }
    if ((conversion_result == 0) &&
        (backend->id == SGL_TEST_RAW_BACKEND_SIMD) &&
        (sgl_test_compare_output(
            resources->reference,
            resources->backend_reference,
            output_size) != 0)) {
        result = 1;
    }
    if (conversion_result == 0) {
        /*
         * Bitwise OR ensures the backend baseline is retained for debugging
         * even when its bytes differ from the generic baseline.
         */
        result |= sgl_test_store_output_case(
            index, operation, backend, thread,
            src_format, dst_format, resources,
            resources->backend_reference, output_size, elapsed_us);
    }

    return result;
}

static int sgl_test_run_thread_comparison_case(
    sgl_size_t index,
    const sgl_test_raw_operation_t *operation,
    const sgl_test_raw_backend_t *backend,
    const sgl_test_raw_thread_t *thread,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    sgl_test_raw_resources_t *resources)
{
    sgl_test_convert_runner_t runner;
    sgl_size_t output_size;
    sgl_uint64_t elapsed_us = 0ULL;
    int conversion_result;
    int compare_result = 0;
    int result = 0;

    runner = sgl_test_get_runner(operation, backend->id);
    output_size = sgl_test_get_raw_size(dst_format, resources);
    (void)sgl_memset(resources->dst, 0, resources->capacity);
    conversion_result = sgl_test_run_conversion(
        runner, thread, src_format, dst_format, resources,
        resources->dst, &elapsed_us);
    result = conversion_result;
    if (conversion_result == 0) {
        compare_result = sgl_test_compare_output(
            resources->backend_reference, resources->dst, output_size);
    }
    if (conversion_result == 0) {
        /*
         * Bitwise OR retains a file-write failure while every worker-count
         * result is stored for benchmark inspection.
         */
        result |= sgl_test_store_output_case(
            index, operation, backend, thread,
            src_format, dst_format, resources,
            resources->dst, output_size, elapsed_us);
    }
    if (compare_result != 0) {
        /* A byte mismatch remains a test failure after both files are saved. */
        result |= 1;
    }

    return result;
}

static int sgl_test_run_backend_cases(
    const sgl_test_raw_operation_t *operation,
    const sgl_test_raw_backend_t *backend,
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    sgl_test_raw_resources_t *resources,
    sgl_size_t *case_index)
{
    sgl_test_raw_thread_t threads[] = {
        {1U, SGL_NULL},
#if defined(SGL_CFG_HAS_THREAD)
        {SGL_TEST_RAW_WORKERS, resources->pool}
#endif
    };
    sgl_size_t thread_index = 1U;
    int result;

    result = sgl_test_run_reference_case(
        *case_index,
        operation,
        backend,
        &threads[0],
        src_format,
        dst_format,
        resources);
    ++(*case_index);
    while (thread_index < SGL_TEST_RAW_ARRAY_SIZE(threads)) {
        /*
         * Bitwise OR retains a mismatch while checking every configured
         * worker count against this backend's one-thread baseline.
         */
        result |= sgl_test_run_thread_comparison_case(
            *case_index,
            operation,
            backend,
            &threads[thread_index],
            src_format,
            dst_format,
            resources);
        ++(*case_index);
        ++thread_index;
    }

    return result;
}

static int sgl_test_run_format_pair(
    const sgl_test_raw_format_t *src_format,
    const sgl_test_raw_format_t *dst_format,
    sgl_test_raw_resources_t *resources,
    sgl_size_t *case_index)
{
    const sgl_test_raw_operation_t *operation;
    sgl_size_t backend_index = 0U;
    int result = 0;

    operation = sgl_test_get_operation(
        src_format->family, dst_format->family);
    (void)sgl_memset(resources->reference, 0, resources->capacity);
    while (backend_index < SGL_TEST_RAW_ARRAY_SIZE(
            sgl_test_raw_backends)) {
        /*
         * Bitwise OR retains a backend failure while continuing through the
         * complete backend and FourCC matrix.
         */
        result |= sgl_test_run_backend_cases(
            operation,
            &sgl_test_raw_backends[backend_index],
            src_format,
            dst_format,
            resources,
            case_index);
        ++backend_index;
    }

    return result;
}

static int sgl_test_run_source_format(
    const sgl_test_raw_format_t *src_format,
    sgl_test_raw_resources_t *resources,
    sgl_size_t *case_index)
{
    sgl_size_t dst_index = 0U;
    int source_result;
    int result = 0;

    source_result = sgl_test_prepare_source_format(src_format, resources);
    if (source_result != 0) {
        result = 1;
    }
    while (dst_index < SGL_TEST_RAW_ARRAY_SIZE(sgl_test_raw_formats)) {
        if (source_result == 0) {
            /*
             * Bitwise OR keeps executing every destination for this source
             * while retaining any earlier destination failure.
             */
            result |= sgl_test_run_format_pair(
                src_format,
                &sgl_test_raw_formats[dst_index],
                resources,
                case_index);
        }
        ++dst_index;
    }

    return result;
}

static int sgl_test_run_complete_matrix(
    sgl_test_raw_resources_t *resources)
{
    sgl_size_t src_index = 0U;
    sgl_size_t case_index = 0U;
    int result = 0;

    while (src_index < SGL_TEST_RAW_ARRAY_SIZE(sgl_test_raw_formats)) {
        /*
         * Bitwise OR preserves a source-format failure while keeping the
         * remaining source formats available for diagnostics.
         */
        result |= sgl_test_run_source_format(
            &sgl_test_raw_formats[src_index],
            resources,
            &case_index);
        ++src_index;
    }

    return result;
}

static sgl_int32_t sgl_test_limit_even_dimension(
    sgl_int32_t source,
    sgl_int32_t requested)
{
    sgl_int32_t result = 0;

    if ((source > 1) && (requested > 1)) {
        sgl_uint32_t limited;

        limited = (sgl_uint32_t)requested;
        if (source < requested) {
            limited = (sgl_uint32_t)source;
        }
        /* Clear the low bit so every 4:2:0 and 4:2:2 case is displayable. */
        limited &= ~1U;
        result = (sgl_int32_t)limited;
    }

    return result;
}

static sgl_fourcc_t sgl_test_get_canonical_format(sgl_int32_t channels)
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

static int sgl_test_configure_resources(
    sgl_test_raw_resources_t *resources)
{
    int result = 0;

    resources->canonical_format = sgl_test_get_canonical_format(
        resources->png->channels);
    resources->width = sgl_test_limit_even_dimension(
        resources->png->width, SGL_TEST_CONVERT_MATRIX_WIDTH);
    resources->height = sgl_test_limit_even_dimension(
        resources->png->height, SGL_TEST_CONVERT_MATRIX_HEIGHT);
    if ((resources->canonical_format == 0U) ||
        (resources->width <= 0) ||
        (resources->height <= 0)) {
        result = 1;
    }
    if (result == 0) {
        resources->pixels =
            (sgl_size_t)(sgl_uint32_t)resources->width *
            (sgl_size_t)(sgl_uint32_t)resources->height;
        /* Four bytes per pixel is the maximum supported format size. */
        resources->capacity = resources->pixels << 2U;
    }

    return result;
}

static int sgl_test_allocate_resources(
    sgl_test_raw_resources_t *resources)
{
    int result = 0;

    resources->canonical = sgl_memory_as_uint8(
        sgl_malloc(resources->capacity));
    resources->src = sgl_memory_as_uint8(
        sgl_malloc(resources->capacity));
    resources->reference = sgl_memory_as_uint8(
        sgl_malloc(resources->capacity));
    resources->backend_reference = sgl_memory_as_uint8(
        sgl_malloc(resources->capacity));
    resources->dst = sgl_memory_as_uint8(
        sgl_malloc(resources->capacity));
    if ((resources->canonical == SGL_NULL) ||
        (resources->src == SGL_NULL) ||
        (resources->reference == SGL_NULL) ||
        (resources->backend_reference == SGL_NULL) ||
        (resources->dst == SGL_NULL)) {
        result = 1;
    }

    return result;
}

static int sgl_test_prepare_canonical_source(
    sgl_test_raw_resources_t *resources)
{
    int result;

    result = sgl_test_prepare_convert_source(
        resources->png,
        resources->canonical,
        resources->width,
        resources->height);

    return result;
}

static int sgl_test_create_threadpool(
    sgl_test_raw_resources_t *resources)
{
    int result = 0;

#if defined(SGL_CFG_HAS_THREAD)
    resources->pool = sgl_threadpool_create(
        SGL_TEST_RAW_WORKERS,
        SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS,
        "convert-matrix");
    if (resources->pool == SGL_NULL) {
        result = 1;
    }
#else
    SGL_UNUSED_PARAM(resources);
#endif

    return result;
}

static int sgl_test_prepare_resources(
    const char *source_path,
    sgl_test_raw_resources_t *resources)
{
    int result = 0;

    resources->png = sgl_test_load_png(source_path);
    if (resources->png == SGL_NULL) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_configure_resources(resources) != 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_allocate_resources(resources) != 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_prepare_canonical_source(resources) != 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_create_threadpool(resources) != 0)) {
        result = 1;
    }
    if (result == 0) {
        resources->command_descriptor = sgl_test_open_output(
            SGL_TEST_RAW_COMMAND_PATH);
        if (resources->command_descriptor < 0) {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_write_command_header(
    const sgl_test_raw_resources_t *resources)
{
    static const char header[] =
        "# Generated convert matrix viewer commands.\n"
        "# Filenames encode case index, backend, operation, FourCC direction,\n"
        "# worker count, and resolution. YUV uses full-range BT.601.\n"
        "# Every backend and worker-count result is saved. Results are also\n"
        "# compared byte-for-byte so mismatches still fail the test.\n"
        "# SGL packed YUV444 has no matching FFmpeg 8-bit input format.\n\n";
    int result;

    result = sgl_test_write_text(resources->command_descriptor, header);

    return result;
}

static int sgl_test_execute_matrix(
    sgl_test_raw_resources_t *resources)
{
    int result;

    result = sgl_test_write_command_header(resources);
    if (result == 0) {
        sgl_test_print_output_header();
        result = sgl_test_run_complete_matrix(resources);
    }

    return result;
}

static int sgl_test_release_resources(
    sgl_test_raw_resources_t *resources)
{
    int result = 0;

    if (resources->command_descriptor >= 0) {
        if (close(resources->command_descriptor) != 0) {
            result = 1;
        }
    }
#if defined(SGL_CFG_HAS_THREAD)
    if (resources->pool != SGL_NULL) {
        if (sgl_threadpool_destroy(resources->pool) != SGL_SUCCESS) {
            result = 1;
        }
    }
#endif
    sgl_free(resources->dst);
    sgl_free(resources->backend_reference);
    sgl_free(resources->reference);
    sgl_free(resources->src);
    sgl_free(resources->canonical);
    if (resources->png != SGL_NULL) {
        sgl_test_release_png(resources->png);
    }

    return result;
}

int sgl_test_write_convert_raw_outputs(const char *source_path)
{
    sgl_test_raw_resources_t resources;
    int result = 0;

    resources.png = SGL_NULL;
    resources.pool = SGL_NULL;
    resources.canonical = SGL_NULL;
    resources.src = SGL_NULL;
    resources.reference = SGL_NULL;
    resources.backend_reference = SGL_NULL;
    resources.dst = SGL_NULL;
    resources.canonical_format = 0U;
    resources.width = 0;
    resources.height = 0;
    resources.pixels = 0U;
    resources.capacity = 0U;
    resources.command_descriptor = -1;
    if ((source_path == SGL_NULL) ||
        (sgl_memory_pool_initialize(
            sgl_test_raw_memory_pool,
            sizeof(sgl_test_raw_memory_pool)) != SGL_SUCCESS)) {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_prepare_resources(source_path, &resources);
    }
    if (result == 0) {
        result = sgl_test_execute_matrix(&resources);
    }
    result |= sgl_test_release_resources(&resources);
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}
