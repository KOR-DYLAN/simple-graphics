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

static unsigned char sgl_test_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

int main(int argc, char *argv[]) {
    sgl_test_png_t *png = NULL;
    int result = 0;

    SGL_UNUSED_PARAM(argc);

    if (sgl_memory_pool_initialize(
            sgl_test_memory_pool,
            sizeof(sgl_test_memory_pool)) != SGL_SUCCESS) {
        result = 1;
    }
    if (result == 0) {
        png = sgl_test_load_png(argv[1]);
        if (png != NULL) {
            sgl_test_save_data("build/image.raw", png->data, png->width * png->height * png->channels);

            sgl_test_save_png(png, "build/clone.png");
            sgl_test_release_png(png);
        }
        else {
            result = 1;
        }
    }
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}
