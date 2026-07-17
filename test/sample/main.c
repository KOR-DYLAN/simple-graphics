/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <stdio.h>
#include <stdint.h>
#include <sgl-core.h>
#include "util.h"

#define SGL_TEST_MEMORY_POOL_SIZE   (32U * 1024U * 1024U)
#ifndef SGL_TEST_BUILD_DIR
#define SGL_TEST_BUILD_DIR          "."
#endif
#define SGL_TEST_SAMPLE_RAW         SGL_TEST_BUILD_DIR "/image.raw"
#define SGL_TEST_SAMPLE_CLONE       SGL_TEST_BUILD_DIR "/clone.png"

static unsigned char sgl_test_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

int main(int argc, char *argv[]) {
    sgl_test_png_t *png = NULL;
    int result = 0;
    int memory_pool_initialized = 0;

    if (argc != 2) {
        (void)fprintf(stderr, "usage: %s <input.png>\n", argv[0]);
        result = 1;
    }

    if ((result == 0) &&
        (sgl_memory_pool_initialize(
            sgl_test_memory_pool,
            sizeof(sgl_test_memory_pool)) != SGL_SUCCESS)) {
        result = 1;
    }
    else if (result == 0) {
        memory_pool_initialized = 1;
    }
    if (result == 0) {
        png = sgl_test_load_png(argv[1]);
        if (png != NULL) {
            if (sgl_test_save_data(
                    SGL_TEST_SAMPLE_RAW,
                    png->data,
                    png->width * png->height * png->channels) != 0) {
                result = 1;
            }

            if (sgl_test_save_png(png, SGL_TEST_SAMPLE_CLONE) != 0) {
                result = 1;
            }
            sgl_test_release_png(png);
        }
        else {
            result = 1;
        }
    }
    if ((memory_pool_initialized != 0) &&
        (sgl_memory_pool_deinitialize() != SGL_SUCCESS)) {
        result = 1;
    }

    return result;
}
