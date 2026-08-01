/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include <sgl-fixed_point.h>

#include "rotate_benchmark.h"

#define SGL_TEST_ARRAY_SIZE(a)    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_ROTATE_WIDTH      (256)
#define SGL_TEST_ROTATE_HEIGHT     (256)
#define SGL_TEST_ROTATE_DST_SIDE   (370)
#define SGL_TEST_ROTATE_DST_BYTES \
    (SGL_TEST_ROTATE_DST_SIDE * SGL_TEST_ROTATE_DST_SIDE)
#define SGL_TEST_ROTATE_POOL_BYTES (2097152U)
#define SGL_TEST_ROTATE_ANGLE      (33.0)
#define SGL_TEST_RANGE_WIDTH       (7)
#define SGL_TEST_RANGE_HEIGHT      (5)
#define SGL_TEST_RANGE_DST_BYTES   (576U)
#define SGL_TEST_RANGE_POOL_BYTES  (32768U)

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

static int sgl_test_rotate_90(void)
{
    const sgl_uint8_t src[] = {
        1U, 2U,
        3U, 4U,
        5U, 6U
    };
    sgl_uint8_t dst[6];
    const sgl_uint8_t expected[] = {
        5U, 3U, 1U,
        6U, 4U, 2U
    };
    int result = 0;

    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL, src, 2, 3, dst, 1, 90.0) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8(
            dst, expected, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_rotate_dimensions(void)
{
    int result = 0;
    sgl_int32_t dst_width = 0;
    sgl_int32_t dst_height = 0;

    if (sgl_get_rotated_dimensions(
            2, 3, 90.0, &dst_width, &dst_height) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) && ((dst_width != 3) || (dst_height != 2))) {
        result = 1;
    }
    if ((result == 0) && (sgl_get_rotated_size(2, 3, 1, 90.0) != 6)) {
        result = 1;
    }

    return result;
}

static int sgl_test_rotate_arbitrary(void)
{
    const sgl_uint8_t src[] = {1U, 2U, 3U, 4U};
    sgl_uint8_t dst[9];
    sgl_int32_t dst_width = 0;
    sgl_int32_t dst_height = 0;
    int result = 0;

    if (sgl_get_rotated_dimensions(
            2, 2, 45.0, &dst_width, &dst_height) != SGL_SUCCESS) {
        result = 1;
    }
    if ((dst_width != 3) || (dst_height != 3)) {
        result = 1;
    }
    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL, src, 2, 2, dst, 1, 45.0) != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}

static int sgl_test_fixed_point(void)
{
    int result = 0;

    if (sgl_fixed_div_pow2_i64(-513, 8U) != -2) {
        result = 1;
    }
    if ((result == 0) && (sgl_fixed_floor_pow2_i64(-1, 11U) != -1)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_q11_ext_mul(
            (sgl_q11_ext_t)(255 * SGL_Q11_ONE),
            (sgl_q11_ext_t)SGL_Q11_ONE) !=
         (sgl_q11_ext_t)(255 * SGL_Q11_ONE))) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_q16_round_to_int32(
            -(sgl_q16_ext_t)SGL_Q16_HALF) != -1)) {
        result = 1;
    }

    return result;
}

static int sgl_test_fixed_point_limits(void)
{
    int result = 0;

    if (sgl_fixed_div_pow2_i64(-513, 64U) != 0) {
        result = 1;
    }
    if (sgl_fixed_floor_pow2_i64(-1, 64U) != -1) {
        result = 1;
    }
    if (sgl_int_to_q11(SGL_FIXED_INT32_MAX) != SGL_FIXED_INT32_MAX) {
        result = 1;
    }
    if (sgl_q11_mul(
            (sgl_q11_t)SGL_FIXED_INT16_MAX,
            (sgl_q11_t)SGL_FIXED_INT16_MAX) !=
        (sgl_q11_t)SGL_FIXED_INT16_MAX) {
        result = 1;
    }
    if (sgl_q11_ext_mul(
            SGL_FIXED_INT32_MAX, SGL_FIXED_INT32_MAX) !=
        SGL_FIXED_INT32_MAX) {
        result = 1;
    }
    if (sgl_q16_round_to_int32(
            (sgl_q16_ext_t)0x7FFFFFFFFFFFFFFFLL) !=
        SGL_FIXED_INT32_MAX) {
        result = 1;
    }
    if (sgl_q16_round_to_int32(
            (sgl_q16_ext_t)(-0x7FFFFFFFFFFFFFFFLL - 1LL)) !=
        SGL_FIXED_INT32_MIN) {
        result = 1;
    }

    return result;
}

static int sgl_test_rotate_invalid_arguments(void)
{
    sgl_int32_t dst_width;
    sgl_int32_t dst_height;
    int result = 0;

    if (sgl_get_rotated_dimensions(
            SGL_FIXED_INT32_MAX, SGL_FIXED_INT32_MAX, 45.0,
            &dst_width, &dst_height) != SGL_ERROR_INVALID_ARGUMENTS) {
        result = 1;
    }

    return result;
}

static int sgl_test_run_rotate_lut_reuse(
    sgl_rotate_lookup_t *lut,
    const sgl_uint8_t *src,
    const sgl_uint8_t *expected,
    sgl_uint8_t *dst)
{
    int result = 0;

    result |= (int)(sgl_generic_rotate(
        SGL_NULL, lut, src, 2, 1, dst, 4, 180.0) != SGL_SUCCESS);
    result |= sgl_test_compare_u8(dst, expected, 8U);
    result |= (int)(sgl_generic_rotate(
        SGL_NULL, lut, src, 2, 1, dst, 4, 90.0) !=
        SGL_ERROR_INVALID_ARGUMENTS);

    return result;
}

static int sgl_test_rotate_lut_reuse(void)
{
    static sgl_uint8_t memory_pool[1024];
    const sgl_uint8_t src[] = {
        1U, 2U, 3U, 4U,
        5U, 6U, 7U, 8U
    };
    const sgl_uint8_t expected[] = {
        5U, 6U, 7U, 8U,
        1U, 2U, 3U, 4U
    };
    sgl_uint8_t dst[8];
    sgl_rotate_lookup_t *lut = SGL_NULL;
    sgl_bool_t initialized = SGL_FALSE;
    int result = 0;

    if (sgl_memory_pool_initialize(
            memory_pool, sizeof(memory_pool)) == SGL_SUCCESS) {
        initialized = SGL_TRUE;
        lut = sgl_generic_create_rotate_lut(2, 1, 180.0);
        if (lut == SGL_NULL) {
            result = 1;
        }
    }
    else {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_run_rotate_lut_reuse(
            lut, src, expected, dst);
    }
    sgl_generic_destroy_rotate_lut(lut);
    if ((initialized == SGL_TRUE) &&
        (sgl_memory_pool_deinitialize() != SGL_SUCCESS)) {
        result = 1;
    }

    return result;
}

static int sgl_test_rotate_direct_ranges(void)
{
    static const sgl_float64_t angles[] = {
        0.0, 0.5, 30.0, 45.0, 60.0, 89.5, 90.0, 120.0,
        179.5, 180.0, 240.0, 269.5, 270.0, 359.5, 360.0
    };
    static sgl_uint8_t memory_pool[SGL_TEST_RANGE_POOL_BYTES];
    sgl_uint8_t src[
        SGL_TEST_RANGE_WIDTH * SGL_TEST_RANGE_HEIGHT * SGL_BPP32];
    sgl_uint8_t direct[SGL_TEST_RANGE_DST_BYTES];
    sgl_uint8_t lookup[SGL_TEST_RANGE_DST_BYTES];
    sgl_rotate_lookup_t *lut = SGL_NULL;
    sgl_size_t index = 0U;
    sgl_size_t angle_index = 0U;
    sgl_bool_t initialized = SGL_FALSE;
    int result = 0;

    while (index < SGL_TEST_ARRAY_SIZE(src)) {
        /* Keep the low byte to create deterministic, nonuniform RGBA data. */
        src[index] = (sgl_uint8_t)((index * 37U) & 0xFFU);
        ++index;
    }
    if (sgl_memory_pool_initialize(
            memory_pool, sizeof(memory_pool)) == SGL_SUCCESS) {
        initialized = SGL_TRUE;
    }
    else {
        result = 1;
    }
    while ((result == 0) &&
           (angle_index < SGL_TEST_ARRAY_SIZE(angles))) {
        sgl_int32_t dst_size;

        dst_size = sgl_get_rotated_size(
            SGL_TEST_RANGE_WIDTH,
            SGL_TEST_RANGE_HEIGHT,
            SGL_BPP32,
            angles[angle_index]);
        if ((dst_size <= 0) ||
            ((sgl_size_t)(sgl_uint32_t)dst_size > sizeof(direct))) {
            result = 1;
        }
        if (result == 0) {
            lut = sgl_generic_create_rotate_lut(
                SGL_TEST_RANGE_WIDTH,
                SGL_TEST_RANGE_HEIGHT,
                angles[angle_index]);
            if (lut == SGL_NULL) {
                result = 1;
            }
        }
        if ((result == 0) &&
            (sgl_generic_rotate(
                 SGL_NULL,
                 SGL_NULL,
                 src,
                 SGL_TEST_RANGE_WIDTH,
                 SGL_TEST_RANGE_HEIGHT,
                 direct,
                 SGL_BPP32,
                 angles[angle_index]) != SGL_SUCCESS)) {
            result = 1;
        }
        if ((result == 0) &&
            (sgl_generic_rotate(
                 SGL_NULL,
                 lut,
                 src,
                 SGL_TEST_RANGE_WIDTH,
                 SGL_TEST_RANGE_HEIGHT,
                 lookup,
                 SGL_BPP32,
                 angles[angle_index]) != SGL_SUCCESS)) {
            result = 1;
        }
        if ((result == 0) &&
            (sgl_test_compare_u8(
                 direct,
                 lookup,
                 (sgl_size_t)(sgl_uint32_t)dst_size) != 0)) {
            result = 1;
        }
        sgl_generic_destroy_rotate_lut(lut);
        lut = SGL_NULL;
        ++angle_index;
    }
    sgl_generic_destroy_rotate_lut(lut);
    if ((initialized == SGL_TRUE) &&
        (sgl_memory_pool_deinitialize() != SGL_SUCCESS)) {
        result = 1;
    }

    return result;
}

#if defined(SGL_CFG_HAS_NEON)
static int sgl_test_simd_rotate(void)
{
    sgl_uint8_t src[17];
    sgl_uint8_t generic[17];
    sgl_uint8_t simd[17];
    sgl_size_t index;
    int result = 0;

    index = 0U;
    while (index < SGL_TEST_ARRAY_SIZE(src)) {
        src[index] = (sgl_uint8_t)(index + 1U);
        ++index;
    }
    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL, src, 17, 1, generic, 1, 180.0) !=
        SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_simd_rotate(
             SGL_NULL, SGL_NULL, src, 17, 1, simd, 1, 180.0) !=
         SGL_SUCCESS)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8(
            generic, simd, SGL_TEST_ARRAY_SIZE(generic)) != 0)) {
        result = 1;
    }
    index = 0U;
    while (index < SGL_TEST_ARRAY_SIZE(generic)) {
        result |= (int)generic[index] ^
            (int)src[SGL_TEST_ARRAY_SIZE(src) - index - 1U];
        ++index;
    }

    return result;
}

static int sgl_test_simd_rotate_channels(void)
{
    const sgl_uint8_t src_u16[] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U,
        10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U
    };
    const sgl_uint8_t src_u32[] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
        9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U
    };
    sgl_uint8_t generic_u16[18];
    sgl_uint8_t simd_u16[18];
    sgl_uint8_t generic_u32[16];
    sgl_uint8_t simd_u32[16];
    int result = 0;

    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL,
            src_u16, 9, 1, generic_u16, 2, 180.0) != SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_simd_rotate(
            SGL_NULL, SGL_NULL,
            src_u16, 9, 1, simd_u16, 2, 180.0) != SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_test_compare_u8(
            generic_u16, simd_u16, sizeof(generic_u16)) != 0) {
        result = 1;
    }
    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL,
            src_u32, 4, 1, generic_u32, 4, 180.0) != SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_simd_rotate(
            SGL_NULL, SGL_NULL,
            src_u32, 4, 1, simd_u32, 4, 180.0) != SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_test_compare_u8(
            generic_u32, simd_u32, sizeof(generic_u32)) != 0) {
        result = 1;
    }

    return result;
}

static int sgl_test_simd_rotate_right_angle_tails(void)
{
    sgl_uint8_t src[5U * 3U * 4U];
    sgl_uint8_t generic[5U * 3U * 4U];
    sgl_uint8_t simd[5U * 3U * 4U];
    sgl_size_t index = 0U;
    int result = 0;

    while (index < SGL_TEST_ARRAY_SIZE(src)) {
        src[index] = (sgl_uint8_t)(index + 1U);
        ++index;
    }
    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL, src, 5, 3, generic, 4, 90.0) !=
        SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_simd_rotate(
            SGL_NULL, SGL_NULL, src, 5, 3, simd, 4, 90.0) !=
        SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_test_compare_u8(
            generic, simd, SGL_TEST_ARRAY_SIZE(generic)) != 0) {
        result = 1;
    }
    if (sgl_generic_rotate(
            SGL_NULL, SGL_NULL, src, 5, 3, generic, 4, 270.0) !=
        SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_simd_rotate(
            SGL_NULL, SGL_NULL, src, 5, 3, simd, 4, 270.0) !=
        SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_test_compare_u8(
            generic, simd, SGL_TEST_ARRAY_SIZE(generic)) != 0) {
        result = 1;
    }

    return result;
}
#endif

#if defined(SGL_CFG_HAS_THREAD)
typedef struct {
    sgl_threadpool_t *pool;
    sgl_rotate_lookup_t *lut;
    sgl_bool_t initialized;
} sgl_test_rotate_resources_t;

static void sgl_test_initialize_threaded_rotate_source(sgl_uint8_t *src)
{
    sgl_size_t index;

    index = 0U;
    while (index < (sgl_size_t)(
            SGL_TEST_ROTATE_WIDTH * SGL_TEST_ROTATE_HEIGHT)) {
        /* Keep only the low eight index bits as a repeatable byte pattern. */
        src[index] = (sgl_uint8_t)(index & 0xFFU);
        ++index;
    }
}

static int sgl_test_run_threaded_rotate(
    sgl_threadpool_t *pool,
    sgl_rotate_lookup_t *lut,
    const sgl_uint8_t *src,
    const sgl_uint8_t *expected,
    sgl_uint8_t *actual,
    sgl_size_t output_size)
{
    int result = 0;

    if (sgl_generic_rotate(
            pool, lut, src,
            SGL_TEST_ROTATE_WIDTH, SGL_TEST_ROTATE_HEIGHT,
            actual, 1, SGL_TEST_ROTATE_ANGLE) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8(expected, actual, output_size) != 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_generic_rotate(
             pool, lut, src,
             SGL_TEST_ROTATE_WIDTH, SGL_TEST_ROTATE_HEIGHT,
             actual, 1, 34.0) != SGL_ERROR_INVALID_ARGUMENTS)) {
        result = 1;
    }

    return result;
}

static sgl_test_rotate_resources_t sgl_test_create_rotate_resources(
    sgl_uint8_t *memory_pool)
{
    sgl_test_rotate_resources_t result;

    result.pool = SGL_NULL;
    result.lut = SGL_NULL;
    result.initialized = SGL_FALSE;
    if (sgl_memory_pool_initialize(
            memory_pool, SGL_TEST_ROTATE_POOL_BYTES) == SGL_SUCCESS) {
        result.initialized = SGL_TRUE;
        result.lut = sgl_generic_create_rotate_lut(
            SGL_TEST_ROTATE_WIDTH, SGL_TEST_ROTATE_HEIGHT,
            SGL_TEST_ROTATE_ANGLE);
        result.pool = sgl_threadpool_create(4U, 1U, "rotate-test");
    }

    return result;
}

static sgl_bool_t sgl_test_rotate_resources_are_valid(
    const sgl_test_rotate_resources_t *resources)
{
    sgl_bool_t result = SGL_FALSE;

    if ((resources->pool != SGL_NULL) &&
        (resources->lut != SGL_NULL)) {
        result = SGL_TRUE;
    }

    return result;
}

static int sgl_test_release_rotate_resources(
    sgl_test_rotate_resources_t *resources)
{
    int result = 0;

    if (resources->pool != SGL_NULL) {
        if (sgl_threadpool_destroy(resources->pool) != SGL_SUCCESS) {
            result = 1;
        }
    }
    sgl_generic_destroy_rotate_lut(resources->lut);
    if (resources->initialized == SGL_TRUE) {
        if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
            result = 1;
        }
    }

    return result;
}

static int sgl_test_prepare_threaded_rotate(
    const sgl_uint8_t *src,
    sgl_uint8_t *expected,
    sgl_int32_t *output_size)
{
    int result = 0;

    *output_size = sgl_get_rotated_size(
        SGL_TEST_ROTATE_WIDTH, SGL_TEST_ROTATE_HEIGHT,
        1, SGL_TEST_ROTATE_ANGLE);
    if ((*output_size <= 0) ||
        ((sgl_size_t)(sgl_uint32_t)*output_size >
         SGL_TEST_ROTATE_DST_BYTES)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_generic_rotate(
             SGL_NULL, SGL_NULL, src,
             SGL_TEST_ROTATE_WIDTH, SGL_TEST_ROTATE_HEIGHT,
             expected, 1, SGL_TEST_ROTATE_ANGLE) != SGL_SUCCESS)) {
        result = 1;
    }

    return result;
}

static int sgl_test_threaded_rotate_lut(void)
{
    static sgl_uint8_t memory_pool[SGL_TEST_ROTATE_POOL_BYTES];
    static sgl_uint8_t src[
        SGL_TEST_ROTATE_WIDTH * SGL_TEST_ROTATE_HEIGHT];
    static sgl_uint8_t expected[SGL_TEST_ROTATE_DST_BYTES];
    static sgl_uint8_t actual[SGL_TEST_ROTATE_DST_BYTES];
    sgl_test_rotate_resources_t resources;
    sgl_int32_t output_size;
    int result = 0;

    resources.pool = SGL_NULL;
    resources.lut = SGL_NULL;
    resources.initialized = SGL_FALSE;
    sgl_test_initialize_threaded_rotate_source(src);
    result = sgl_test_prepare_threaded_rotate(
        src, expected, &output_size);
    if (result == 0) {
        resources = sgl_test_create_rotate_resources(memory_pool);
        if (sgl_test_rotate_resources_are_valid(&resources) != SGL_TRUE) {
            result = 1;
        }
    }
    if (result == 0) {
        result = sgl_test_run_threaded_rotate(
            resources.pool, resources.lut, src, expected, actual,
            (sgl_size_t)(sgl_uint32_t)output_size);
    }
    result |= sgl_test_release_rotate_resources(&resources);

    return result;
}
#endif  /* SGL_CFG_HAS_THREAD */

static int sgl_test_run_all(void)
{
    static const sgl_test_function_t tests[] = {
        sgl_test_rotate_90,
        sgl_test_rotate_dimensions,
        sgl_test_rotate_arbitrary,
        sgl_test_fixed_point,
        sgl_test_fixed_point_limits,
        sgl_test_rotate_invalid_arguments,
        sgl_test_rotate_lut_reuse,
        sgl_test_rotate_direct_ranges,
#if defined(SGL_CFG_HAS_NEON)
        sgl_test_simd_rotate,
        sgl_test_simd_rotate_channels,
        sgl_test_simd_rotate_right_angle_tails,
#endif
#if defined(SGL_CFG_HAS_THREAD)
        sgl_test_threaded_rotate_lut,
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
        (sgl_test_run_rotate_benchmark(argv[1]) != 0)) {
        result = 1;
    }

    return result;
}
