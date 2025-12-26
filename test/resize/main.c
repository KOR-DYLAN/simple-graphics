#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "util.h"
#include "sgl.h"

typedef enum {
    SGL_TEST_RESIZE_NEAREST,
    SGL_TEST_RESIZE_BILINEAR,
    SGL_TEST_RESIZE_BILINEAR_SIMD,
} sgl_test_resize_method_t;

typedef enum {
    SGL_TEST_THREADPOOL_COUNT_1,
    SGL_TEST_THREADPOOL_COUNT_2,
    SGL_TEST_THREADPOOL_COUNT_4,
    SGL_TEST_THREADPOOL_COUNT_8,
    MAX_SGL_TEST_THREADPOOL_COUNT,
} sgl_test_threadpool_count_t;

typedef struct {
    uint8_t *buf;
    int32_t width;
    int32_t height;
    int32_t bpp;
} sgl_test_resize_source_t;

typedef struct {
    int32_t width;
    int32_t height;
    sgl_test_resize_method_t method;
    sgl_test_threadpool_count_t num_threads;
} sgl_test_resize_t;

static const char *resize_method_name[] = {
    "nearest",
    "bilinear",
    "bilinear-simd",
};

static const size_t threadpool_count_table[MAX_SGL_TEST_THREADPOOL_COUNT] = {
    1, 2, 4, 8,
};

static const sgl_test_resize_t resize_test_vector[] = {
    { .width  = 640, .height =  480, .method = SGL_TEST_RESIZE_NEAREST,         .num_threads = SGL_TEST_THREADPOOL_COUNT_1 },
    { .width = 1280, .height =  720, .method = SGL_TEST_RESIZE_NEAREST,         .num_threads = SGL_TEST_THREADPOOL_COUNT_1 },
    { .width = 1920, .height = 1080, .method = SGL_TEST_RESIZE_NEAREST,         .num_threads = SGL_TEST_THREADPOOL_COUNT_1 },
    { .width = 2560, .height = 1440, .method = SGL_TEST_RESIZE_NEAREST,         .num_threads = SGL_TEST_THREADPOOL_COUNT_1 },

    { .width  = 640, .height =  480, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width  = 640, .height =  480, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_2, },
    { .width  = 640, .height =  480, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_4, },
    { .width  = 640, .height =  480, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_8, },

    { .width = 1280, .height =  720, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width = 1280, .height =  720, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_2 },
    { .width = 1280, .height =  720, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_4 },
    { .width = 1280, .height =  720, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_8 },

    { .width = 1920, .height = 1080, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width = 1920, .height = 1080, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_2 },
    { .width = 1920, .height = 1080, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_4 },
    { .width = 1920, .height = 1080, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_8 },

    { .width = 2560, .height = 1440, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width = 2560, .height = 1440, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_2 },
    { .width = 2560, .height = 1440, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_4 },
    { .width = 2560, .height = 1440, .method = SGL_TEST_RESIZE_BILINEAR,        .num_threads = SGL_TEST_THREADPOOL_COUNT_8 },

    { .width  = 640, .height =  480, .method = SGL_TEST_RESIZE_BILINEAR_SIMD,   .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width = 1280, .height =  720, .method = SGL_TEST_RESIZE_BILINEAR_SIMD,   .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width = 1920, .height = 1080, .method = SGL_TEST_RESIZE_BILINEAR_SIMD,   .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
    { .width = 2560, .height = 1440, .method = SGL_TEST_RESIZE_BILINEAR_SIMD,   .num_threads = SGL_TEST_THREADPOOL_COUNT_1, },
};

static void sgl_run_resize_test_vector(sgl_test_resize_source_t *src);

int main(int argc, char *argv[]) {
    sgl_test_png_t *png = NULL;
    sgl_test_resize_source_t src;

    (void)argc;

    png = sgl_test_load_png(argv[1]);
    if (png != NULL) {
        src.buf = png->data;
        src.width = png->width;
        src.height = png->height;
        src.bpp = png->channels;
        sgl_run_resize_test_vector(&src);

        sgl_test_release_png(png);
    }

    return 0;
}

static void sgl_run_resize_test_vector(sgl_test_resize_source_t *src)
{
    int32_t i;
    uint8_t *dst;
    int32_t imgsize;
    sgl_test_resize_method_t method;
    sgl_threadpool_t *pool[MAX_SGL_TEST_THREADPOOL_COUNT] = { NULL}; 
    sgl_test_threadpool_count_t num_threads;
    char path[FILENAME_MAX];
    sgl_test_png_t resize_png;
    uint64_t timestamp_us, elapsed_us;

    for (num_threads = SGL_TEST_THREADPOOL_COUNT_2; num_threads < MAX_SGL_TEST_THREADPOOL_COUNT; ++num_threads) {
        pool[num_threads] = sgl_threadpool_create(threadpool_count_table[num_threads], SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS, "resize_pool");
        assert(pool[num_threads] != NULL);
    }

    printf("################################################################\n");
    printf("                    Resize Test Vector                          \n");
    printf("################################################################\n");
    for (i = 0; i < (int32_t)(sizeof(resize_test_vector) / sizeof(sgl_test_resize_t)); ++i) {
        method = resize_test_vector[i].method;
        imgsize = resize_test_vector[i].width * resize_test_vector[i].height * src->bpp;
        dst = (uint8_t *)calloc(1, (size_t)imgsize);
        assert(dst != NULL);

        num_threads = resize_test_vector[i].num_threads;
        timestamp_us = sgl_test_get_timestamp_us(0);
        switch (method) {
        case SGL_TEST_RESIZE_NEAREST:
            (void)sgl_generic_resize_nearest(dst, resize_test_vector[i].width, resize_test_vector[i].height, src->buf, src->width, src->height, src->bpp);
            break;
        case SGL_TEST_RESIZE_BILINEAR:
            (void)sgl_generic_resize_bilinear(pool[num_threads], NULL, dst, resize_test_vector[i].width, resize_test_vector[i].height, src->buf, src->width, src->height, src->bpp);
            break;
        case SGL_TEST_RESIZE_BILINEAR_SIMD:
            (void)sgl_simd_resize_bilinear(pool[num_threads], NULL, dst, resize_test_vector[i].width, resize_test_vector[i].height, src->buf, src->width, src->height, src->bpp);
            break;
        }
        elapsed_us = sgl_test_get_timestamp_us(timestamp_us);
        printf("[%3d]  %4llu.%03llums, %16s, %dx%d, %zu thread\n", 
                i, elapsed_us / 1000ULL, elapsed_us % 1000ULL, 
                resize_method_name[method], 
                resize_test_vector[i].width, resize_test_vector[i].height,
                threadpool_count_table[num_threads]);

        sprintf(path, "build/%03d_resize_%s_%dx%d_%zuthread.png", 
                i, 
                resize_method_name[method], 
                resize_test_vector[i].width, resize_test_vector[i].height,
                threadpool_count_table[num_threads]);
        resize_png.data = dst;
        resize_png.width = resize_test_vector[i].width;
        resize_png.height = resize_test_vector[i].height;
        resize_png.channels = src->bpp;
        sgl_test_save_png(&resize_png, path);
        free(dst);
    }

    for (num_threads = SGL_TEST_THREADPOOL_COUNT_2; num_threads < MAX_SGL_TEST_THREADPOOL_COUNT; ++num_threads) {
        sgl_threadpool_destroy(pool[num_threads]);
    }
}
