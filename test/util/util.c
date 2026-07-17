/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <png.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sgl-core.h>
#include "util.h"
#include <sgl_memory_cast.h>

#define SGL_TEST_PNG_COMPRESSION_LEVEL  (1)
#define SGL_TEST_PNG_WRITE_BUFFER_SIZE  (1024U * 1024U)

typedef struct {
    png_structp png;
    png_infop info;
    FILE *fp;
} png_t;

static SGL_ALWAYS_INLINE sgl_test_png_t *sgl_test_memory_as_png(void *memory)
{
    sgl_test_png_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (sgl_test_png_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE png_t *sgl_test_memory_as_png_handle(void *memory)
{
    png_t *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (png_t *)memory;

    return result;
}

static SGL_ALWAYS_INLINE png_bytep *sgl_test_memory_as_png_rows(void *memory)
{
    png_bytep *result;

    /* SGL-MEM-DEV-001: typed conversion from generic storage. */
    /* cppcheck-suppress misra-c2012-11.5 */
    result = (png_bytep *)memory;

    return result;
}

static png_t *sgl_test_png_read_init(const char *path);
static png_t *sgl_test_png_write_init(const char *path);
static void sgl_test_png_read_deinit(png_t *handle);
static void sgl_test_png_write_deinit(png_t *handle);
static png_voidp sgl_test_png_malloc(png_structp png, png_alloc_size_t size);
static void sgl_test_png_free(png_structp png, png_voidp memory);

static png_voidp sgl_test_png_malloc(png_structp png, png_alloc_size_t size)
{
    png_voidp memory;

    SGL_UNUSED_PARAM(png);
    memory = sgl_malloc((size_t)size);

    return memory;
}

static void sgl_test_png_free(png_structp png, png_voidp memory)
{
    SGL_UNUSED_PARAM(png);
    sgl_free(memory);
}

sgl_test_png_t *sgl_test_load_png(const char *path)
{
    sgl_test_png_t *test_handle = NULL;
    png_t *handle = NULL;

    png_byte color_type;
    png_byte bit_depth;
    int32_t rowbytes;
    png_bytep *row_pointers;
    int32_t row;

    handle = sgl_test_png_read_init(path);
    if (handle != NULL) {
        png_read_info(handle->png, handle->info);

        test_handle = sgl_test_memory_as_png(sgl_malloc(sizeof(sgl_test_png_t)));
        if (test_handle != NULL) {
            color_type = png_get_color_type(handle->png, handle->info);
            bit_depth  = png_get_bit_depth(handle->png, handle->info);

            if (bit_depth == 16) {
                png_set_strip_16(handle->png);
            }

            if (color_type == PNG_COLOR_TYPE_PALETTE) {
                png_set_palette_to_rgb(handle->png);
            }

            if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
                png_set_expand_gray_1_2_4_to_8(handle->png);
            }

            if (png_get_valid(handle->png, handle->info, PNG_INFO_tRNS)) {
                png_set_tRNS_to_alpha(handle->png);
            }

            png_read_update_info(handle->png, handle->info);

            test_handle->width = (int32_t)png_get_image_width(handle->png, handle->info);
            test_handle->height = (int32_t)png_get_image_height(handle->png, handle->info);
            test_handle->channels = (int32_t)png_get_channels(handle->png, handle->info);

            rowbytes = (int32_t)png_get_rowbytes(handle->png, handle->info);
            test_handle->data = sgl_memory_as_uint8(
                sgl_malloc((size_t)rowbytes * (size_t)test_handle->height));
            if (test_handle->data != NULL) {
                row_pointers = sgl_test_memory_as_png_rows(
                    sgl_malloc(sizeof(png_bytep) * (size_t)test_handle->height));
                if (row_pointers != NULL) {
                    for (row = 0; row < test_handle->height; row++) {
                        row_pointers[row] = test_handle->data + (row * rowbytes);
                    }

                    png_read_image(handle->png, row_pointers);

                    sgl_test_png_read_deinit(handle);
                    sgl_free(row_pointers);
                }
                else {
                    sgl_test_png_read_deinit(handle);
                }
            }
            else {
                sgl_test_png_read_deinit(handle);
            }
        }
        else {
            sgl_test_png_read_deinit(handle);
        }
    }

    return test_handle;
}

int32_t sgl_test_save_png(sgl_test_png_t *png, const char *path)
{
    png_t *handle;
    int color_type;
    size_t rowbytes;
    png_bytep *row_pointers;
    int32_t row;

    if ((png == NULL) || (path == NULL)) {
        return -1;
    }
    if ((png->channels < 1) || (png->channels > 4)) {
        return -1;
    }

    handle = sgl_test_png_write_init(path);
    if (!handle) {
        return -1;
    }

    if (setjmp(png_jmpbuf(handle->png))) {
        sgl_test_png_write_deinit(handle);
        return -1;
    }

    if (png->channels == 1) {
        color_type = PNG_COLOR_TYPE_GRAY;
    }
    else if (png->channels == 2) {
        color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
    }
    else if (png->channels == 3) {
        color_type = PNG_COLOR_TYPE_RGB;
    }
    else {
        color_type = PNG_COLOR_TYPE_RGBA;
    }
    png_set_IHDR(handle->png, handle->info,
                 (png_uint_32)png->width, (png_uint_32)png->height,
                 8, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(handle->png, SGL_TEST_PNG_COMPRESSION_LEVEL);
    png_set_filter(handle->png, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE);
    png_write_info(handle->png, handle->info);

    /* rowbytes in bytes per row, check overflow */
    rowbytes = (size_t)png->width * (size_t)png->channels;
    row_pointers = sgl_test_memory_as_png_rows(
        sgl_malloc(sizeof(png_bytep) * (size_t)png->height));
    if (row_pointers == NULL) {
        sgl_test_png_write_deinit(handle);
        return -1;
    }

    for (row = 0; row < png->height; row++) {
        row_pointers[row] = (png_bytep)(png->data + (size_t)row * rowbytes);
    }

    png_write_image(handle->png, row_pointers);
    png_write_end(handle->png, NULL);

    sgl_test_png_write_deinit(handle);
    sgl_free(row_pointers);

    return 0;
}


void sgl_test_release_png(sgl_test_png_t *png)
{
    if (png != NULL) {
        if (png->data != NULL) {
            sgl_free(png->data);
        }
        sgl_free(png);
    }
}

int32_t sgl_test_save_data(const char *path, uint8_t *data, int32_t datasize)
{
    FILE *fp;
    int32_t result = 0;

    fp = fopen(path, "wb");
    if (fp != NULL) {
        (void)fwrite(data, (size_t)datasize, 1, fp);
        (void)fclose(fp);
    }
    else {
        result = -1;
    }

    return result;
}

static png_t *sgl_test_png_read_init(const char *path)
{
    png_t *handle = NULL;
    int32_t result;

    handle = sgl_test_memory_as_png_handle(sgl_malloc(sizeof(png_t)));
    if (handle != NULL) {
        handle->png = png_create_read_struct_2(
            PNG_LIBPNG_VER_STRING,
            NULL,
            NULL,
            NULL,
            NULL,
            sgl_test_png_malloc,
            sgl_test_png_free);
        if (handle->png != NULL) {
            handle->info = png_create_info_struct(handle->png);
            if (handle->info != NULL) {
                result = setjmp(png_jmpbuf(handle->png));
                if (result == 0) {
                    handle->fp = fopen(path, "rb");
                    if (handle->fp != NULL) {
                        png_init_io(handle->png, handle->fp);
                    }
                    else {
                        sgl_test_png_read_deinit(handle);
                    }
                }
                else {
                    sgl_test_png_read_deinit(handle);
                }
            }
            else {
                sgl_test_png_read_deinit(handle);
            }
        }
        else {
            sgl_test_png_read_deinit(handle);
        }
    }

    return handle;
}

static png_t *sgl_test_png_write_init(const char *path)
{
    png_t *handle = sgl_test_memory_as_png_handle(sgl_malloc(sizeof(png_t)));
    if (!handle) {
        return NULL;
    }

    handle->png = png_create_write_struct_2(
        PNG_LIBPNG_VER_STRING,
        NULL,
        NULL,
        NULL,
        NULL,
        sgl_test_png_malloc,
        sgl_test_png_free);
    if (!handle->png) {
        sgl_test_png_write_deinit(handle);
        return NULL;
    }

    handle->info = png_create_info_struct(handle->png);
    if (!handle->info) {
        sgl_test_png_write_deinit(handle);
        return NULL;
    }

    if (setjmp(png_jmpbuf(handle->png))) {
        sgl_test_png_write_deinit(handle);
        return NULL;
    }

    handle->fp = fopen(path, "wb");
    if (!handle->fp) {
        sgl_test_png_write_deinit(handle);
        return NULL;
    }
    (void)setvbuf(handle->fp, NULL, _IOFBF, SGL_TEST_PNG_WRITE_BUFFER_SIZE);

    png_init_io(handle->png, handle->fp);

    return handle;
}


static void sgl_test_png_read_deinit(png_t *handle)
{
    if (handle != NULL) {
        if ((handle->png != NULL) || (handle->info != NULL)) {
            png_destroy_read_struct(&handle->png, &handle->info, NULL);
        }

        if (handle->fp != NULL) {
            fclose(handle->fp);
        }

        sgl_free(handle);
    }
}

static void sgl_test_png_write_deinit(png_t *handle)
{
    if (handle != NULL) {
        if ((handle->png != NULL) || (handle->info != NULL)) {
            png_destroy_write_struct(&handle->png, &handle->info);
        }

        if (handle->fp != NULL) {
            fclose(handle->fp);
        }

        sgl_free(handle);
    }
}

uint64_t sgl_test_get_timestamp_us(uint64_t start_us)
{
    struct timespec timespec;
    uint64_t val;

    (void)clock_gettime(CLOCK_MONOTONIC, &timespec);
    val = ((uint64_t)timespec.tv_sec * 1000000ULL) + ((uint64_t)timespec.tv_nsec / 1000ULL);

    if (0ULL < start_us) {
        val -= start_us;
    }

    return val;
}
