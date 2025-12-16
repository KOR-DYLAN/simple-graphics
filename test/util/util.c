#include <png.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include "util.h"

typedef struct {
    png_structp png;
    png_infop info;
    FILE *fp;
} png_t;

static png_t *sgl_test_png_read_init(const char *path);
static png_t *sgl_test_png_write_init(const char *path);
static void sgl_test_png_read_deinit(png_t *handle);
static void sgl_test_png_write_deinit(png_t *handle);

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

        test_handle = (sgl_test_png_t *)malloc(sizeof(sgl_test_png_t));
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

            if ((color_type == PNG_COLOR_TYPE_RGB)  ||
                (color_type == PNG_COLOR_TYPE_GRAY) ||
                (color_type == PNG_COLOR_TYPE_PALETTE)) {
                png_set_filler(handle->png, 0xFF, PNG_FILLER_AFTER);
            }

            if ((color_type == PNG_COLOR_TYPE_GRAY) ||
                (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)) {
                png_set_gray_to_rgb(handle->png);
            }

            png_read_update_info(handle->png, handle->info);

            test_handle->width = (int32_t)png_get_image_width(handle->png, handle->info);
            test_handle->height = (int32_t)png_get_image_height(handle->png, handle->info);
            test_handle->channels = (int32_t)png_get_channels(handle->png, handle->info);

            rowbytes = (int32_t)png_get_rowbytes(handle->png, handle->info);
            test_handle->data = (unsigned char*)malloc((size_t)rowbytes * (size_t)test_handle->height);
            if (test_handle->data != NULL) {
                row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * (size_t)test_handle->height);
                if (row_pointers != NULL) {
                    for (row = 0; row < test_handle->height; row++) {
                        row_pointers[row] = test_handle->data + (row * rowbytes);
                    }

                    png_read_image(handle->png, row_pointers);

                    sgl_test_png_read_deinit(handle);
                    free(row_pointers);
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
    int32_t result = 0;
    png_t *handle;
    int32_t rowbytes;
    int32_t color_type;
    png_bytep *row_pointers;
    int32_t row;

    if ((png != NULL) && (path != NULL)) {
        if ((png->channels == 3) || (png->channels == 4)) {
            handle = sgl_test_png_write_init(path);
            if (handle != NULL) {
                color_type = (png->channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
                png_set_IHDR(handle->png, handle->info, (png_uint_32)png->width, (png_uint_32)png->height,
                             8, color_type, PNG_INTERLACE_NONE,
                             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
                png_write_info(handle->png, handle->info);

                rowbytes = png->width * png->channels;
                row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * (size_t)png->height);
                if (row_pointers != NULL) {
                    for (row = 0; row < png->height; row++) {
                        row_pointers[row] = (png_bytep)(png->data + (row * rowbytes));
                    }

                    png_write_image(handle->png, row_pointers);
                    png_write_end(handle->png, NULL);

                    sgl_test_png_write_deinit(handle);
                    free(row_pointers);
                }
                else {
                    sgl_test_png_write_deinit(handle);
                    result = -1;
                }
            }
            else {
                result = -1;
            }
        }
        else {
            result = -1;
        }
    }
    else {
        result = -1;
    }

    return result;
}

void sgl_test_release_png(sgl_test_png_t *png)
{
    if (png != NULL) {
        if (png->data != NULL) {
            free (png->data);
        }
        free(png);
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

    handle = (png_t *)malloc(sizeof(png_t));
    if (handle != NULL) {
        handle->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
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
    png_t *handle = NULL;
    int32_t result;

    handle = (png_t *)malloc(sizeof(png_t));
    if (handle != NULL) {
        handle->png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (handle->png != NULL) {
            handle->info = png_create_info_struct(handle->png);
            if (handle->info != NULL) {
                result = setjmp(png_jmpbuf(handle->png));
                if (result == 0) {
                    handle->fp = fopen(path, "wb");
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

static void sgl_test_png_read_deinit(png_t *handle)
{
    if (handle != NULL) {
        if ((handle->png != NULL) || (handle->info != NULL)) {
            png_destroy_read_struct(&handle->png, &handle->info, NULL);
        }

        if (handle->fp != NULL) {
            fclose(handle->fp);
        }

        free(handle);
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

        free(handle);
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
