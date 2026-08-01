/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>

#include "convert_benchmark.h"
#include "raw_output.h"

#define SGL_TEST_ARRAY_SIZE(a)    ((sgl_size_t)(sizeof(a) / sizeof((a)[0])))
#define SGL_TEST_RGB_PIXELS       (16)
#define SGL_TEST_RGB_BYTES        (SGL_TEST_RGB_PIXELS * SGL_BPP32)
#define SGL_TEST_CONVERT_WIDTH    (512)
#define SGL_TEST_CONVERT_HEIGHT   (256)
#define SGL_TEST_CONVERT_PIXELS \
    (SGL_TEST_CONVERT_WIDTH * SGL_TEST_CONVERT_HEIGHT)
#define SGL_TEST_CONVERT_RGB_BYTES    (SGL_TEST_CONVERT_PIXELS * SGL_BPP32)
#define SGL_TEST_CONVERT_YUV444_BYTES (SGL_TEST_CONVERT_PIXELS * 3)
#define SGL_TEST_CONVERT_YUV420_BYTES \
    (SGL_TEST_CONVERT_PIXELS + (SGL_TEST_CONVERT_PIXELS / 2))
#define SGL_TEST_CONVERT_POOL_BYTES   (262144U)
#define SGL_TEST_SIMD_YUV_WIDTH       (16)
#define SGL_TEST_SIMD_YUV_HEIGHT      (2)
#define SGL_TEST_SIMD_YUV_PIXELS      \
    (SGL_TEST_SIMD_YUV_WIDTH * SGL_TEST_SIMD_YUV_HEIGHT)
#define SGL_TEST_SIMD_YUV_BYTES       (SGL_TEST_SIMD_YUV_PIXELS * 3)
#define SGL_TEST_SIMD_RGBA_BYTES      (SGL_TEST_SIMD_YUV_PIXELS * SGL_BPP32)

typedef int (*sgl_test_function_t)(void);

static int sgl_test_compare_u8_impl(
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

static int sgl_test_compare_expected_u8(
    const sgl_uint8_t *actual,
    const sgl_uint8_t *expected,
    sgl_size_t count)
{
    int result;

    result = sgl_test_compare_u8_impl(actual, expected, count);

    return result;
}

#if defined(SGL_CFG_HAS_NEON)
static int sgl_test_compare_simd_u8(
    const sgl_uint8_t *generic,
    const sgl_uint8_t *simd,
    sgl_size_t count)
{
    int result;

    result = sgl_test_compare_u8_impl(generic, simd, count);

    return result;
}
#endif

static int sgl_test_compare_layout_u8(
    const sgl_uint8_t *actual,
    const sgl_uint8_t *expected,
    sgl_size_t count)
{
    int result;

    result = sgl_test_compare_u8_impl(actual, expected, count);

    return result;
}

static int sgl_test_fourcc(void)
{
    int result = 0;

    if (SGL_FOURCC_RGBA != SGL_FOURCC('A', 'B', '2', '4')) {
        result = 1;
    }
    if (SGL_FOURCC_ARGB != SGL_FOURCC('B', 'A', '2', '4')) {
        result = 1;
    }
    if (SGL_FOURCC_RGB != SGL_FOURCC('R', 'G', 'B', '3')) {
        result = 1;
    }
    if (SGL_FOURCC_YUYV != SGL_FOURCC('Y', 'U', 'Y', 'V')) {
        result = 1;
    }
    if (SGL_FOURCC_YUV420_PLANAR != SGL_FOURCC('Y', 'U', '1', '2')) {
        result = 1;
    }
    if (SGL_FOURCC_NV12 != SGL_FOURCC('N', 'V', '1', '2')) {
        result = 1;
    }

    return result;
}

static int sgl_test_rgb_to_rgb_planar(void)
{
    const sgl_uint8_t src[] = {
        10U, 20U, 30U,
        40U, 50U, 60U
    };
    sgl_uint8_t dst[6];
    const sgl_uint8_t expected[] = {
        30U, 60U,
        20U, 50U,
        10U, 40U
    };
    int result = 0;

    if (sgl_generic_rgb2rgb(
            SGL_NULL, src, SGL_FOURCC_RGB,
            dst, SGL_FOURCC_BGR_PLANAR, 2, 1) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_expected_u8(
            dst, expected, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_rgb_to_yuv444(void)
{
    const sgl_uint8_t src[] = {
        10U, 10U, 10U,
        255U, 255U, 255U
    };
    sgl_uint8_t dst[6];
    const sgl_uint8_t expected[] = {
        10U, 128U, 128U,
        255U, 128U, 128U
    };
    int result = 0;

    if (sgl_generic_rgb2yuv(
            SGL_NULL, src, SGL_FOURCC_RGB,
            dst, SGL_FOURCC_YUV444_PACKED, 2, 1) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_expected_u8(
            dst, expected, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_yuv444_to_rgb(void)
{
    const sgl_uint8_t src[] = {
        20U, 128U, 128U,
        200U, 128U, 128U
    };
    sgl_uint8_t dst[6];
    const sgl_uint8_t expected[] = {
        20U, 20U, 20U,
        200U, 200U, 200U
    };
    int result = 0;

    if (sgl_generic_yuv2rgb(
            SGL_NULL, src, SGL_FOURCC_YUV444_PACKED,
            dst, SGL_FOURCC_RGB, 2, 1) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_expected_u8(
            dst, expected, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_yuv444_to_nv12(void)
{
    const sgl_uint8_t src[] = {
        10U, 100U, 140U,
        20U, 110U, 150U,
        30U, 120U, 160U,
        40U, 130U, 170U
    };
    sgl_uint8_t dst[6];
    const sgl_uint8_t expected[] = {
        10U, 20U, 30U, 40U,
        115U, 155U
    };
    int result = 0;

    if (sgl_generic_yuv2yuv(
            SGL_NULL, src, SGL_FOURCC_YUV444_PACKED,
            dst, SGL_FOURCC_NV12, 2, 2) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_expected_u8(
            dst, expected, SGL_TEST_ARRAY_SIZE(expected)) != 0)) {
        result = 1;
    }

    return result;
}

static int sgl_test_rgb_layouts(void)
{
    const sgl_uint8_t src[] = {
        1U, 2U, 3U, 4U,
        5U, 6U, 7U, 8U
    };
    const sgl_uint8_t rgba[] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    const sgl_uint8_t argb[] = {4U, 1U, 2U, 3U, 8U, 5U, 6U, 7U};
    const sgl_uint8_t bgra[] = {3U, 2U, 1U, 4U, 7U, 6U, 5U, 8U};
    const sgl_uint8_t abgr[] = {4U, 3U, 2U, 1U, 8U, 7U, 6U, 5U};
    const sgl_uint8_t rgb[] = {1U, 2U, 3U, 5U, 6U, 7U};
    const sgl_uint8_t bgr[] = {3U, 2U, 1U, 7U, 6U, 5U};
    const sgl_uint8_t rgba_planar[] = {1U, 5U, 2U, 6U, 3U, 7U, 4U, 8U};
    const sgl_uint8_t argb_planar[] = {4U, 8U, 1U, 5U, 2U, 6U, 3U, 7U};
    const sgl_uint8_t bgra_planar[] = {3U, 7U, 2U, 6U, 1U, 5U, 4U, 8U};
    const sgl_uint8_t abgr_planar[] = {4U, 8U, 3U, 7U, 2U, 6U, 1U, 5U};
    const sgl_uint8_t rgb_planar[] = {1U, 5U, 2U, 6U, 3U, 7U};
    const sgl_uint8_t bgr_planar[] = {3U, 7U, 2U, 6U, 1U, 5U};
    const sgl_fourcc_t formats[] = {
        SGL_FOURCC_RGBA, SGL_FOURCC_ARGB,
        SGL_FOURCC_BGRA, SGL_FOURCC_ABGR,
        SGL_FOURCC_RGB, SGL_FOURCC_BGR,
        SGL_FOURCC_RGBA_PLANAR, SGL_FOURCC_ARGB_PLANAR,
        SGL_FOURCC_BGRA_PLANAR, SGL_FOURCC_ABGR_PLANAR,
        SGL_FOURCC_RGB_PLANAR, SGL_FOURCC_BGR_PLANAR
    };
    const sgl_uint8_t *expected[] = {
        rgba, argb, bgra, abgr, rgb, bgr,
        rgba_planar, argb_planar, bgra_planar, abgr_planar,
        rgb_planar, bgr_planar
    };
    const sgl_size_t sizes[] = {
        8U, 8U, 8U, 8U, 6U, 6U,
        8U, 8U, 8U, 8U, 6U, 6U
    };
    sgl_uint8_t dst[8];
    sgl_size_t index = 0U;
    int result = 0;

    while (index < SGL_TEST_ARRAY_SIZE(formats)) {
        if (sgl_generic_rgb2rgb(
                SGL_NULL, src, SGL_FOURCC_RGBA,
                dst, formats[index], 2, 1) !=
            SGL_SUCCESS) {
            result = 1;
        }
        if (sgl_test_compare_layout_u8(
                dst, expected[index], sizes[index]) != 0) {
            result = 1;
        }
        ++index;
    }

    return result;
}

static int sgl_test_yuv_layouts(void)
{
    const sgl_uint8_t src[] = {
        10U, 100U, 150U, 20U, 110U, 160U,
        30U, 120U, 170U, 40U, 130U, 180U,
        50U, 140U, 190U, 60U, 150U, 200U,
        70U, 160U, 210U, 80U, 170U, 220U
    };
    const sgl_uint8_t yuv444_planar[] = {
        10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U,
        100U, 110U, 120U, 130U, 140U, 150U, 160U, 170U,
        150U, 160U, 170U, 180U, 190U, 200U, 210U, 220U
    };
    const sgl_uint8_t yuyv[] = {
        10U, 105U, 20U, 155U, 30U, 125U, 40U, 175U,
        50U, 145U, 60U, 195U, 70U, 165U, 80U, 215U
    };
    const sgl_uint8_t yvyu[] = {
        10U, 155U, 20U, 105U, 30U, 175U, 40U, 125U,
        50U, 195U, 60U, 145U, 70U, 215U, 80U, 165U
    };
    const sgl_uint8_t uyvy[] = {
        105U, 10U, 155U, 20U, 125U, 30U, 175U, 40U,
        145U, 50U, 195U, 60U, 165U, 70U, 215U, 80U
    };
    const sgl_uint8_t vyuy[] = {
        155U, 10U, 105U, 20U, 175U, 30U, 125U, 40U,
        195U, 50U, 145U, 60U, 215U, 70U, 165U, 80U
    };
    const sgl_uint8_t yuv422_planar[] = {
        10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U,
        105U, 125U, 145U, 165U,
        155U, 175U, 195U, 215U
    };
    const sgl_uint8_t yuv420_planar[] = {
        10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U,
        125U, 145U, 175U, 195U
    };
    const sgl_uint8_t nv12[] = {
        10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U,
        125U, 175U, 145U, 195U
    };
    const sgl_uint8_t nv21[] = {
        10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U,
        175U, 125U, 195U, 145U
    };
    const sgl_fourcc_t formats[] = {
        SGL_FOURCC_YUV444_PLANAR,
        SGL_FOURCC_YUYV, SGL_FOURCC_YVYU,
        SGL_FOURCC_UYVY, SGL_FOURCC_VYUY,
        SGL_FOURCC_YUV422_PLANAR,
        SGL_FOURCC_YUV420_PLANAR,
        SGL_FOURCC_NV12, SGL_FOURCC_NV21
    };
    const sgl_uint8_t *expected[] = {
        yuv444_planar, yuyv, yvyu, uyvy, vyuy,
        yuv422_planar, yuv420_planar, nv12, nv21
    };
    const sgl_size_t sizes[] = {
        24U, 16U, 16U, 16U, 16U, 16U, 12U, 12U, 12U
    };
    sgl_uint8_t dst[24];
    sgl_size_t index = 0U;
    int result = 0;

    while (index < SGL_TEST_ARRAY_SIZE(formats)) {
        if (sgl_generic_yuv2yuv(
                SGL_NULL, src, SGL_FOURCC_YUV444_PACKED, dst,
                formats[index], 4, 2) != SGL_SUCCESS) {
            result = 1;
        }
        if (sgl_test_compare_layout_u8(
                dst, expected[index], sizes[index]) != 0) {
            result = 1;
        }
        ++index;
    }

    return result;
}

static int sgl_test_invalid_arguments(void)
{
    const sgl_uint8_t src[4] = {0U, 0U, 0U, 0U};
    sgl_uint8_t dst[4];
    int result = 0;

    if (sgl_generic_rgb2yuv(
            SGL_NULL, src, SGL_FOURCC_RGB,
            dst, SGL_FOURCC_RGB, 1, 1) !=
        SGL_ERROR_NOT_SUPPORTED) {
        result = 1;
    }

    return result;
}

#if defined(SGL_CFG_HAS_NEON)
static void sgl_test_fill_rgba(sgl_uint8_t *buffer)
{
    sgl_int32_t pixel;

    pixel = 0;
    while (pixel < SGL_TEST_RGB_PIXELS) {
        sgl_size_t offset;
        sgl_uint32_t value;

        offset = (sgl_size_t)(sgl_uint32_t)pixel * 4U;
        value = (sgl_uint32_t)pixel * 17U;
        /* Masking to eight low bits is modulo 256 before narrowing. */
        value &= 0xFFU;
        buffer[offset] = (sgl_uint8_t)value;
        value = (sgl_uint32_t)pixel * 29U;
        value &= 0xFFU;
        buffer[offset + 1U] = (sgl_uint8_t)value;
        value = (sgl_uint32_t)pixel * 43U;
        value &= 0xFFU;
        buffer[offset + 2U] = (sgl_uint8_t)value;
        value = 255U - (sgl_uint32_t)pixel;
        buffer[offset + 3U] = (sgl_uint8_t)value;
        ++pixel;
    }
}

static int sgl_test_simd_rgb_matrix(void)
{
    static const sgl_fourcc_t formats[] = {
        SGL_FOURCC_RGBA, SGL_FOURCC_ARGB,
        SGL_FOURCC_BGRA, SGL_FOURCC_ABGR,
        SGL_FOURCC_RGB, SGL_FOURCC_BGR,
        SGL_FOURCC_RGBA_PLANAR, SGL_FOURCC_ARGB_PLANAR,
        SGL_FOURCC_BGRA_PLANAR, SGL_FOURCC_ABGR_PLANAR,
        SGL_FOURCC_RGB_PLANAR, SGL_FOURCC_BGR_PLANAR
    };
    sgl_uint8_t rgba[SGL_TEST_RGB_BYTES];
    sgl_uint8_t src[SGL_TEST_RGB_BYTES];
    sgl_uint8_t generic[SGL_TEST_RGB_BYTES];
    sgl_uint8_t simd[SGL_TEST_RGB_BYTES];
    sgl_size_t src_index;
    int result = 0;

    sgl_test_fill_rgba(rgba);
    src_index = 0U;
    while (src_index < SGL_TEST_ARRAY_SIZE(formats)) {
        sgl_size_t dst_index;
        sgl_result_t operation_result;

        operation_result = sgl_generic_rgb2rgb(
            SGL_NULL, rgba, SGL_FOURCC_RGBA, src, formats[src_index],
            SGL_TEST_RGB_PIXELS, 1);
        if (operation_result != SGL_SUCCESS) {
            result = 1;
        }
        dst_index = 0U;
        while (dst_index < SGL_TEST_ARRAY_SIZE(formats)) {
            (void)sgl_memset(generic, 0xA5, sizeof(generic));
            (void)sgl_memset(simd, 0xA5, sizeof(simd));
            operation_result = sgl_generic_rgb2rgb(
                SGL_NULL, src, formats[src_index],
                generic, formats[dst_index],
                SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            operation_result = sgl_simd_rgb2rgb(
                SGL_NULL, src, formats[src_index],
                simd, formats[dst_index],
                SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            result |= sgl_test_compare_simd_u8(
                generic, simd, SGL_TEST_ARRAY_SIZE(generic));
            ++dst_index;
        }
        ++src_index;
    }

    return result;
}

static int sgl_test_simd_rgb_yuv444(void)
{
    const sgl_fourcc_t rgb_formats[] = {
        SGL_FOURCC_RGBA, SGL_FOURCC_BGR_PLANAR
    };
    const sgl_fourcc_t yuv_formats[] = {
        SGL_FOURCC_YUV444_PACKED, SGL_FOURCC_YUV444_PLANAR
    };
    const sgl_size_t rgb_sizes[] = {
        64U,
        48U
    };
    sgl_uint8_t rgba[SGL_TEST_RGB_BYTES];
    sgl_uint8_t rgb[SGL_TEST_RGB_BYTES];
    sgl_uint8_t yuv_generic[SGL_TEST_RGB_PIXELS * 3];
    sgl_uint8_t yuv_simd[SGL_TEST_RGB_PIXELS * 3];
    sgl_uint8_t rgb_generic[SGL_TEST_RGB_BYTES];
    sgl_uint8_t rgb_simd[SGL_TEST_RGB_BYTES];
    sgl_size_t rgb_index;
    int result = 0;

    sgl_test_fill_rgba(rgba);
    rgb_index = 0U;
    while (rgb_index < 2U) {
        sgl_size_t yuv_index;
        sgl_result_t operation_result;

        operation_result = sgl_generic_rgb2rgb(
            SGL_NULL, rgba, SGL_FOURCC_RGBA,
            rgb, rgb_formats[rgb_index],
            SGL_TEST_RGB_PIXELS, 1);
        if (operation_result != SGL_SUCCESS) {
            result = 1;
        }
        yuv_index = 0U;
        while (yuv_index < 2U) {
            operation_result = sgl_generic_rgb2yuv(
                SGL_NULL, rgb, rgb_formats[rgb_index], yuv_generic,
                yuv_formats[yuv_index], SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            operation_result = sgl_simd_rgb2yuv(
                SGL_NULL, rgb, rgb_formats[rgb_index], yuv_simd,
                yuv_formats[yuv_index], SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            result |= sgl_test_compare_simd_u8(
                yuv_generic, yuv_simd, sizeof(yuv_generic));
            operation_result = sgl_generic_yuv2rgb(
                SGL_NULL, yuv_generic, yuv_formats[yuv_index], rgb_generic,
                rgb_formats[rgb_index], SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            operation_result = sgl_simd_yuv2rgb(
                SGL_NULL, yuv_generic, yuv_formats[yuv_index], rgb_simd,
                rgb_formats[rgb_index], SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            result |= sgl_test_compare_simd_u8(
                rgb_generic, rgb_simd, rgb_sizes[rgb_index]);
            ++yuv_index;
        }
        ++rgb_index;
    }

    return result;
}

static int sgl_test_simd_yuv_matrix(void)
{
    static const sgl_fourcc_t formats[] = {
        SGL_FOURCC_YUV444_PACKED, SGL_FOURCC_YUV444_PLANAR
    };
    sgl_uint8_t packed[SGL_TEST_RGB_PIXELS * 3];
    sgl_uint8_t src[SGL_TEST_RGB_PIXELS * 3];
    sgl_uint8_t generic[SGL_TEST_RGB_PIXELS * 3];
    sgl_uint8_t simd[SGL_TEST_RGB_PIXELS * 3];
    sgl_size_t index;
    sgl_size_t src_index;
    int result = 0;

    index = 0U;
    while (index < SGL_TEST_ARRAY_SIZE(packed)) {
        sgl_uint32_t value;

        value = (sgl_uint32_t)index * 13U;
        /* Masking to eight low bits is modulo 256 before narrowing. */
        value &= 0xFFU;
        packed[index] = (sgl_uint8_t)value;
        ++index;
    }
    src_index = 0U;
    while (src_index < SGL_TEST_ARRAY_SIZE(formats)) {
        sgl_size_t dst_index;
        sgl_result_t operation_result;

        operation_result = sgl_generic_yuv2yuv(
            SGL_NULL, packed, SGL_FOURCC_YUV444_PACKED,
            src, formats[src_index],
            SGL_TEST_RGB_PIXELS, 1);
        if (operation_result != SGL_SUCCESS) {
            result = 1;
        }
        dst_index = 0U;
        while (dst_index < SGL_TEST_ARRAY_SIZE(formats)) {
            operation_result = sgl_generic_yuv2yuv(
                SGL_NULL, src, formats[src_index],
                generic, formats[dst_index],
                SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            operation_result = sgl_simd_yuv2yuv(
                SGL_NULL, src, formats[src_index],
                simd, formats[dst_index],
                SGL_TEST_RGB_PIXELS, 1);
            if (operation_result != SGL_SUCCESS) {
                result = 1;
            }
            result |= sgl_test_compare_simd_u8(
                generic, simd, SGL_TEST_ARRAY_SIZE(generic));
            ++dst_index;
        }
        ++src_index;
    }

    return result;
}

static const sgl_fourcc_t sgl_test_simd_yuv_formats[] = {
    SGL_FOURCC_YUV444_PACKED, SGL_FOURCC_YUV444_PLANAR,
    SGL_FOURCC_YUYV, SGL_FOURCC_YVYU,
    SGL_FOURCC_UYVY, SGL_FOURCC_VYUY,
    SGL_FOURCC_YUV422_PLANAR,
    SGL_FOURCC_YUV420_PLANAR,
    SGL_FOURCC_NV12, SGL_FOURCC_NV21
};

static void sgl_test_initialize_simd_rgba(sgl_uint8_t *rgba)
{
    sgl_size_t pixel = 0U;

    while (pixel < SGL_TEST_SIMD_YUV_PIXELS) {
        sgl_size_t offset;

        offset = pixel << 2U;
        rgba[offset] = (sgl_uint8_t)((pixel * 17U) & 0xFFU);
        rgba[offset + 1U] = (sgl_uint8_t)((pixel * 29U) & 0xFFU);
        rgba[offset + 2U] = (sgl_uint8_t)((pixel * 43U) & 0xFFU);
        rgba[offset + 3U] = 255U;
        ++pixel;
    }
}

static int sgl_test_simd_rgb_to_yuv_matrix(const sgl_uint8_t *rgba)
{
    sgl_uint8_t generic[SGL_TEST_SIMD_YUV_BYTES];
    sgl_uint8_t simd[SGL_TEST_SIMD_YUV_BYTES];
    sgl_size_t index = 0U;
    int result = 0;

    while (index < SGL_TEST_ARRAY_SIZE(sgl_test_simd_yuv_formats)) {
        (void)sgl_memset(generic, 0xA5, sizeof(generic));
        (void)sgl_memset(simd, 0xA5, sizeof(simd));
        if (sgl_generic_rgb2yuv(
                SGL_NULL, rgba, SGL_FOURCC_RGBA, generic,
                sgl_test_simd_yuv_formats[index],
                SGL_TEST_SIMD_YUV_WIDTH,
                SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
            result = 1;
        }
        if (sgl_simd_rgb2yuv(
                SGL_NULL, rgba, SGL_FOURCC_RGBA, simd,
                sgl_test_simd_yuv_formats[index],
                SGL_TEST_SIMD_YUV_WIDTH,
                SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
            result = 1;
        }
        result |= sgl_test_compare_simd_u8(
            generic, simd, sizeof(generic));
        ++index;
    }

    return result;
}

static int sgl_test_simd_yuv_source_matrix(
    const sgl_uint8_t *rgba,
    sgl_fourcc_t src_format)
{
    sgl_uint8_t src[SGL_TEST_SIMD_YUV_BYTES];
    sgl_uint8_t generic[SGL_TEST_SIMD_RGBA_BYTES];
    sgl_uint8_t simd[SGL_TEST_SIMD_RGBA_BYTES];
    sgl_size_t dst_index = 0U;
    int result = 0;

    if (sgl_generic_rgb2yuv(
            SGL_NULL, rgba, SGL_FOURCC_RGBA, src, src_format,
            SGL_TEST_SIMD_YUV_WIDTH,
            SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
        result = 1;
    }
    (void)sgl_memset(generic, 0xA5, sizeof(generic));
    (void)sgl_memset(simd, 0xA5, sizeof(simd));
    if (sgl_generic_yuv2rgb(
            SGL_NULL, src, src_format, generic, SGL_FOURCC_RGBA,
            SGL_TEST_SIMD_YUV_WIDTH,
            SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
        result = 1;
    }
    if (sgl_simd_yuv2rgb(
            SGL_NULL, src, src_format, simd, SGL_FOURCC_RGBA,
            SGL_TEST_SIMD_YUV_WIDTH,
            SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
        result = 1;
    }
    result |= sgl_test_compare_simd_u8(generic, simd, sizeof(generic));
    while (dst_index < SGL_TEST_ARRAY_SIZE(sgl_test_simd_yuv_formats)) {
        (void)sgl_memset(generic, 0xA5, SGL_TEST_SIMD_YUV_BYTES);
        (void)sgl_memset(simd, 0xA5, SGL_TEST_SIMD_YUV_BYTES);
        if (sgl_generic_yuv2yuv(
                SGL_NULL, src, src_format, generic,
                sgl_test_simd_yuv_formats[dst_index],
                SGL_TEST_SIMD_YUV_WIDTH,
                SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
            result = 1;
        }
        if (sgl_simd_yuv2yuv(
                SGL_NULL, src, src_format, simd,
                sgl_test_simd_yuv_formats[dst_index],
                SGL_TEST_SIMD_YUV_WIDTH,
                SGL_TEST_SIMD_YUV_HEIGHT) != SGL_SUCCESS) {
            result = 1;
        }
        result |= sgl_test_compare_simd_u8(
            generic, simd, SGL_TEST_SIMD_YUV_BYTES);
        ++dst_index;
    }

    return result;
}

static int sgl_test_simd_subsampled_yuv(void)
{
    sgl_uint8_t rgba[SGL_TEST_SIMD_RGBA_BYTES];
    sgl_size_t src_index = 0U;
    int result;

    sgl_test_initialize_simd_rgba(rgba);
    result = sgl_test_simd_rgb_to_yuv_matrix(rgba);
    while (src_index < SGL_TEST_ARRAY_SIZE(sgl_test_simd_yuv_formats)) {
        result |= sgl_test_simd_yuv_source_matrix(
            rgba, sgl_test_simd_yuv_formats[src_index]);
        ++src_index;
    }

    return result;
}
#endif  /* SGL_CFG_HAS_NEON */

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_test_initialize_threaded_convert_source(sgl_uint8_t *src)
{
    sgl_size_t index;

    index = 0U;
    while (index < SGL_TEST_CONVERT_RGB_BYTES) {
        /* Keep only the low eight index bits as a repeatable byte pattern. */
        src[index] = (sgl_uint8_t)(index & 0xFFU);
        ++index;
    }
}

static int sgl_test_threaded_generic_convert(
    sgl_threadpool_t *pool,
    const sgl_uint8_t *src,
    sgl_uint8_t *expected,
    sgl_uint8_t *actual)
{
    int result = 0;

    if (sgl_generic_rgb2yuv(
            SGL_NULL, src, SGL_FOURCC_RGBA,
            expected, SGL_FOURCC_NV12,
            SGL_TEST_CONVERT_WIDTH, SGL_TEST_CONVERT_HEIGHT) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_generic_rgb2yuv(
             pool, src, SGL_FOURCC_RGBA,
             actual, SGL_FOURCC_NV12,
             SGL_TEST_CONVERT_WIDTH,
             SGL_TEST_CONVERT_HEIGHT) != SGL_SUCCESS)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8_impl(
             expected, actual, SGL_TEST_CONVERT_YUV420_BYTES) != 0)) {
        result = 1;
    }

    return result;
}

#if defined(SGL_CFG_HAS_NEON)
static int sgl_test_threaded_simd_convert(
    sgl_threadpool_t *pool,
    const sgl_uint8_t *src,
    sgl_uint8_t *expected,
    sgl_uint8_t *actual)
{
    int result = 0;

    if (sgl_simd_rgb2yuv(
            SGL_NULL, src, SGL_FOURCC_RGBA,
            expected, SGL_FOURCC_YUV444_PLANAR,
            SGL_TEST_CONVERT_WIDTH, SGL_TEST_CONVERT_HEIGHT) != SGL_SUCCESS) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_simd_rgb2yuv(
             pool, src, SGL_FOURCC_RGBA,
             actual, SGL_FOURCC_YUV444_PLANAR,
             SGL_TEST_CONVERT_WIDTH,
             SGL_TEST_CONVERT_HEIGHT) != SGL_SUCCESS)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_compare_u8_impl(
             expected, actual, SGL_TEST_CONVERT_YUV444_BYTES) != 0)) {
        result = 1;
    }

    return result;
}
#endif  /* SGL_CFG_HAS_NEON */

static sgl_threadpool_t *sgl_test_create_convert_pool(
    sgl_uint8_t *memory_pool,
    sgl_bool_t *initialized)
{
    sgl_threadpool_t *result = SGL_NULL;

    if (sgl_memory_pool_initialize(
            memory_pool, SGL_TEST_CONVERT_POOL_BYTES) == SGL_SUCCESS) {
        *initialized = SGL_TRUE;
        result = sgl_threadpool_create(4U, 1U, "convert-test");
    }

    return result;
}

static int sgl_test_release_convert_pool(
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

static int sgl_test_threaded_convert(void)
{
    static sgl_uint8_t memory_pool[SGL_TEST_CONVERT_POOL_BYTES];
    static sgl_uint8_t src[SGL_TEST_CONVERT_RGB_BYTES];
    static sgl_uint8_t expected[SGL_TEST_CONVERT_YUV444_BYTES];
    static sgl_uint8_t actual[SGL_TEST_CONVERT_YUV444_BYTES];
    sgl_threadpool_t *pool = SGL_NULL;
    sgl_bool_t initialized = SGL_FALSE;
    int result = 0;

    sgl_test_initialize_threaded_convert_source(src);
    pool = sgl_test_create_convert_pool(memory_pool, &initialized);
    if (pool == SGL_NULL) {
        result = 1;
    }
    if (result == 0) {
        result = sgl_test_threaded_generic_convert(
            pool, src, expected, actual);
    }
#if defined(SGL_CFG_HAS_NEON)
    if (result == 0) {
        result = sgl_test_threaded_simd_convert(
            pool, src, expected, actual);
    }
#endif  /* SGL_CFG_HAS_NEON */
    result |= sgl_test_release_convert_pool(pool, initialized);

    return result;
}
#endif  /* SGL_CFG_HAS_THREAD */

static int sgl_test_run_all(void)
{
    static const sgl_test_function_t tests[] = {
        sgl_test_fourcc,
        sgl_test_rgb_to_rgb_planar,
        sgl_test_rgb_to_yuv444,
        sgl_test_yuv444_to_rgb,
        sgl_test_yuv444_to_nv12,
        sgl_test_rgb_layouts,
        sgl_test_yuv_layouts,
        sgl_test_invalid_arguments,
#if defined(SGL_CFG_HAS_NEON)
        sgl_test_simd_rgb_matrix,
        sgl_test_simd_rgb_yuv444,
        sgl_test_simd_yuv_matrix,
        sgl_test_simd_subsampled_yuv,
#endif
#if defined(SGL_CFG_HAS_THREAD)
        sgl_test_threaded_convert,
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
        (sgl_test_run_convert_benchmark(argv[1]) != 0)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_test_write_convert_raw_outputs(argv[1]) != 0)) {
        result = 1;
    }

    return result;
}
