/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>

#include "crop_benchmark.h"

#define SGL_TEST_ARRAY_SIZE(a)    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_INT32_MAX         (2147483647)
#define SGL_TEST_CROP_SRC_WIDTH    (1024)
#define SGL_TEST_CROP_SRC_HEIGHT   (512)
#define SGL_TEST_CROP_WIDTH        (512)
#define SGL_TEST_CROP_HEIGHT       (512)
#define SGL_TEST_CROP_POOL_BYTES   (262144U)

typedef int (*sgl_test_function_t)(void);

static int sgl_test_compare_u8(
    const sgl_uint8_t *lhs,
    const sgl_uint8_t *rhs,
    sgl_size_t count)
{
    sgl_size_t index;
    int result = 0;

    index = 0U;
    while (index < count) {
        /*
         * XOR sets only differing byte bits; OR retains every mismatch without
         * changing loop control or adding an early-exit branch.
         */
        result |= (int)lhs[index] ^ (int)rhs[index];
        ++index;
    }

    return result;
}

static int sgl_test_crop_region(void)
{
    const sgl_uint8_t src[] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
        9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U,
        17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U
    };
    sgl_uint8_t dst[8];
    const sgl_uint8_t expected[] = {
        11U, 12U, 13U, 14U,
        19U, 20U, 21U, 22U
    };
    int result = 0;

    if (sgl_generic_crop(
            SGL_NULL, src, 4, 3, dst, 1, 1, 2, 2, 2) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8(
            dst, expected, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_crop_invalid_arguments(void)
{
    const sgl_uint8_t src[4] = {0U, 0U, 0U, 0U};
    sgl_uint8_t dst[4];
    int result = 0;

    if (sgl_generic_crop(SGL_NULL, src, 2, 2, dst, 1, 1, 2, 2, 1) !=
        SGL_ERROR_INVALID_ARGUMENTS) {
        result = 1;
    }
    if (sgl_generic_crop(
            SGL_NULL, src, SGL_TEST_INT32_MAX, SGL_TEST_INT32_MAX, dst,
            0, 0, 1, 1, SGL_TEST_INT32_MAX) !=
        SGL_ERROR_INVALID_ARGUMENTS) {
        result = 1;
    }

    return result;
}

#if defined(SGL_CFG_HAS_NEON)
static int sgl_test_simd_crop(void)
{
    sgl_uint8_t src[64];
    sgl_uint8_t generic[36];
    sgl_uint8_t simd[36];
    sgl_size_t index;
    int result = 0;

    index = 0U;
    while (index < SGL_TEST_ARRAY_SIZE(src)) {
        src[index] = (sgl_uint8_t)index;
        ++index;
    }
    if (sgl_generic_crop(
            SGL_NULL, src, 32, 2, generic, 7, 0, 18, 2, 1) !=
        SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_simd_crop(
             SGL_NULL, src, 32, 2, simd, 7, 0, 18, 2, 1) !=
         SGL_SUCCESS)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8(
            generic, simd, SGL_TEST_ARRAY_SIZE(generic)) != 0)) {
        result = 1;
    }

    return result;
}
#endif

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_test_initialize_threaded_crop_source(sgl_uint8_t *src)
{
    sgl_size_t index;

    index = 0U;
    while (index < (sgl_size_t)(
            SGL_TEST_CROP_SRC_WIDTH * SGL_TEST_CROP_SRC_HEIGHT)) {
        /* Keep only the low eight index bits as a repeatable byte pattern. */
        src[index] = (sgl_uint8_t)(index & 0xFFU);
        ++index;
    }
}

static int sgl_test_release_crop_pool(
    sgl_threadpool_t *pool,
    sgl_bool_t initialized)
{
    int result = 0;

    if (pool != SGL_NULL) {
        if (sgl_threadpool_destroy(pool) != SGL_SUCCESS) {
            result = 1;
        }
    }
    if (initialized == SGL_TRUE) {
        if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_run_crop_with_pool(
    sgl_uint8_t *memory_pool,
    const sgl_uint8_t *src,
    sgl_uint8_t *actual)
{
    sgl_threadpool_t *pool = SGL_NULL;
    sgl_bool_t initialized = SGL_FALSE;
    int result = 0;

    if (sgl_memory_pool_initialize(
            memory_pool, SGL_TEST_CROP_POOL_BYTES) != SGL_SUCCESS) {
        result = 1;
    }
    else {
        initialized = SGL_TRUE;
        pool = sgl_threadpool_create(4U, 1U, "crop-test");
        if (pool == SGL_NULL) {
            result = 1;
        }
    }
    if ((result == 0) &&
        (sgl_generic_crop(
             pool, src,
             SGL_TEST_CROP_SRC_WIDTH, SGL_TEST_CROP_SRC_HEIGHT,
             actual, 127, 0,
             SGL_TEST_CROP_WIDTH, SGL_TEST_CROP_HEIGHT, 1) != SGL_SUCCESS)) {
        result = 1;
    }
    result |= sgl_test_release_crop_pool(pool, initialized);

    return result;
}

static int sgl_test_threaded_crop(void)
{
    static sgl_uint8_t memory_pool[SGL_TEST_CROP_POOL_BYTES];
    static sgl_uint8_t src[
        SGL_TEST_CROP_SRC_WIDTH * SGL_TEST_CROP_SRC_HEIGHT];
    static sgl_uint8_t expected[
        SGL_TEST_CROP_WIDTH * SGL_TEST_CROP_HEIGHT];
    static sgl_uint8_t actual[
        SGL_TEST_CROP_WIDTH * SGL_TEST_CROP_HEIGHT];
    int result = 0;

    sgl_test_initialize_threaded_crop_source(src);
    if (sgl_generic_crop(
            SGL_NULL, src,
            SGL_TEST_CROP_SRC_WIDTH, SGL_TEST_CROP_SRC_HEIGHT,
            expected, 127, 0,
            SGL_TEST_CROP_WIDTH, SGL_TEST_CROP_HEIGHT, 1) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_run_crop_with_pool(
             memory_pool, src, actual) != 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8(
             expected, actual, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}
#endif  /* SGL_CFG_HAS_THREAD */

static int sgl_test_run_all(void)
{
    static const sgl_test_function_t tests[] = {
        sgl_test_crop_region,
        sgl_test_crop_invalid_arguments,
#if defined(SGL_CFG_HAS_NEON)
        sgl_test_simd_crop,
#endif
#if defined(SGL_CFG_HAS_THREAD)
        sgl_test_threaded_crop,
#endif
    };
    sgl_size_t index;
    int result = 0;

    index = 0U;
    while (index < SGL_TEST_ARRAY_SIZE(tests)) {
        /* Bitwise OR keeps running every test while retaining any failure. */
        result |= tests[index]();
        ++index;
    }

    return result;
}

int main(int argc, char *argv[])
{
    int result;

    result = sgl_test_run_all();
    if (argc != 2) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_run_crop_benchmark(argv[1]) != 0)) {
        result = 1;
    }

    return result;
}
