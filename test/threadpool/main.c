#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"
#include "sgl.h"

typedef struct {
    uint8_t *rgb;
    uint8_t *yuyv;
    int32_t width;
    int32_t height;
    int32_t bpp;
} sgl_test_convert_data_t;

typedef struct {
    int32_t row;
} sgl_test_convert_current_t;

static void rgb2yuyv_line_stripe(void *current, void *cookie) {
    sgl_test_convert_data_t *data = (sgl_test_convert_data_t *)cookie;
    sgl_test_convert_current_t *cur = (sgl_test_convert_current_t *)current;
    int32_t col;

    for (col = 0; col < data->width; ++col) {
        int32_t rgb_idx = (cur->row * data->width + col) * data->bpp;
        int32_t yuyv_idx = (cur->row * data->width + col) * 2;
        uint8_t r = data->rgb[rgb_idx];
        uint8_t g = data->rgb[rgb_idx + 1];
        uint8_t b = data->rgb[rgb_idx + 2];

        // Convert RGB to YUV
        uint8_t y = (uint8_t)((0.299 * r) + (0.587 * g) + (0.114 * b));
        uint8_t u = (uint8_t)((-0.14713 * r) - (0.28886 * g) + (0.436 * b) + 128);
        uint8_t v = (uint8_t)((0.615 * r) - (0.51498 * g) - (0.10001 * b) + 128);

        // Store YUV values
        data->yuyv[yuyv_idx] = y;
        if ((col % 2) == 0) {
            data->yuyv[yuyv_idx + 1] = u;
        }
        else {
            data->yuyv[yuyv_idx + 1] = v;
        }
    }
}

static void sgl_run_convert_rgb2yuv_single_thread(uint8_t *rgb, uint8_t *yuyv, int32_t width, int32_t height, int32_t bpp) {
    sgl_test_convert_data_t data = {
        .rgb = rgb,
        .yuyv = yuyv,
        .width = width,
        .height = height,
        .bpp = bpp,
    };
    sgl_test_convert_current_t current;

    for (current.row = 0; current.row < height; ++current.row) {
        rgb2yuyv_line_stripe(&current, &data);
    }
}

static void sgl_run_convert_rgb2yuv_multi_thread(uint8_t *rgb, uint8_t *yuyv, int32_t width, int32_t height, int32_t bpp) {
    sgl_test_convert_data_t data = {
        .rgb = rgb,
        .yuyv = yuyv,
        .width = width,
        .height = height,
        .bpp = bpp,
    };
    sgl_threadpool_t *pool = NULL;
    sgl_queue_t *operations = NULL;
    size_t row;

    pool = sgl_threadpool_create(4, (size_t)height, "rgb2yuyv_pool");
    assert(pool != NULL);

    operations = sgl_queue_create((size_t)height);
    assert(operations != NULL);

    for (row = 0; row < (size_t)height; ++row) {
        sgl_test_convert_current_t *current = (sgl_test_convert_current_t *)malloc(sizeof(sgl_test_convert_current_t));
        assert(current != NULL);
        current->row = (int32_t)row;
        sgl_queue_enqueue(operations, (const void *)current);
    }

    sgl_threadpool_attach_routine(pool, rgb2yuyv_line_stripe, operations, (void *)&data);

    for (row = 0; row < (size_t)height; ++row) {
        sgl_test_convert_current_t *current = (sgl_test_convert_current_t *)sgl_queue_dequeue(operations);
        free(current);
    }

    sgl_queue_destroy(&operations);
    sgl_threadpool_destroy(pool);
}

int main(int argc, char *argv[]) {
    char filename[256];
    sgl_test_png_t *png = NULL;
    uint8_t *yuyv = NULL;
    size_t image_size;
    uint64_t timestamp_us, elapsed_us;

    SGL_UNUSED_PARAM(argc);

    png = sgl_test_load_png(argv[1]);
    if (png != NULL) {
        image_size = (size_t)(png->width * png->height * 2);
        yuyv = (uint8_t *)malloc(image_size);
        assert(yuyv != NULL);

        printf("################################################################\n");
        printf("               RGB to YUYV Conversion Test                     \n");
        printf("################################################################\n");

        printf("run single-threaded conversion...\n");
        memset(yuyv, 0, image_size);
        timestamp_us = sgl_test_get_timestamp_us(0);
        sgl_run_convert_rgb2yuv_single_thread(png->data, yuyv, png->width, png->height, png->channels);
        elapsed_us = sgl_test_get_timestamp_us(timestamp_us);
        sprintf(filename, "single_%dx%d.yuv", png->width, png->height);
        printf("[single]  %4llu.%03llums, %dx%d\n", elapsed_us / 1000ULL, elapsed_us % 1000ULL, png->width, png->height);
        sgl_test_save_data(filename, yuyv, (int32_t)image_size);

        printf("run multi-threaded conversion...\n");
        memset(yuyv, 0, image_size);
        timestamp_us = sgl_test_get_timestamp_us(0);
        sgl_run_convert_rgb2yuv_multi_thread(png->data, yuyv, png->width, png->height, png->channels);
        elapsed_us = sgl_test_get_timestamp_us(timestamp_us);
        printf("[multi]   %4llu.%03llums, %dx%d\n", elapsed_us / 1000ULL, elapsed_us % 1000ULL, png->width, png->height);
        sprintf(filename, "multi_%dx%d.yuv", png->width, png->height);
        sgl_test_save_data(filename, yuyv, (int32_t)image_size);

        sgl_test_release_png(png);
        free(yuyv);

        printf("Done.\n");
    }

    return 0;
}
