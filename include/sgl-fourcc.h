/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_FOURCC_H_
#define SGL_FOURCC_H_

/**
 * @file sgl-fourcc.h
 * @brief Standard and SGL-specific pixel-format identifiers.
 */

#include <sgl-type.h>

/** 32-bit pixel-format identifier created by #SGL_FOURCC. */
typedef sgl_uint32_t sgl_fourcc_t;

/** @defgroup sgl_fourcc_formats Pixel formats
 *  @brief FourCC values accepted by SGL conversion functions.
 *
 * All formats use tightly packed rows. Planar formats store each complete
 * plane contiguously in the documented order. `Cb` and `Cr` are also commonly
 * named `U` and `V`.
 *  @{
 */
/**
 * @brief Creates a DRM/V4L2-compatible FourCC value.
 *
 * @p a occupies bits 7:0, @p b bits 15:8, @p c bits 23:16, and @p d bits
 * 31:24. Casting each character to eight bits prevents sign extension before
 * the shifts.
 */
#define SGL_FOURCC(a, b, c, d) \
    ((sgl_fourcc_t)((sgl_uint8_t)(a)) | \
     ((sgl_fourcc_t)((sgl_uint8_t)(b)) << 8U) | \
     ((sgl_fourcc_t)((sgl_uint8_t)(c)) << 16U) | \
     ((sgl_fourcc_t)((sgl_uint8_t)(d)) << 24U))

/** V4L2 RGBA32 / DRM ABGR8888 code; packed bytes `R, G, B, A`. */
#define SGL_FOURCC_RGBA             SGL_FOURCC('A', 'B', '2', '4')
/** V4L2 ARGB32 / DRM BGRA8888 code; packed bytes `A, R, G, B`. */
#define SGL_FOURCC_ARGB             SGL_FOURCC('B', 'A', '2', '4')
/** V4L2 ABGR32 / DRM ARGB8888 code; packed bytes `B, G, R, A`. */
#define SGL_FOURCC_BGRA             SGL_FOURCC('A', 'R', '2', '4')
/** V4L2 BGRA32 / DRM RGBA8888 code; packed bytes `A, B, G, R`. */
#define SGL_FOURCC_ABGR             SGL_FOURCC('R', 'A', '2', '4')
/** V4L2 RGB24 code; packed bytes `R, G, B`. */
#define SGL_FOURCC_RGB              SGL_FOURCC('R', 'G', 'B', '3')
/** V4L2 BGR24 code; packed bytes `B, G, R`. */
#define SGL_FOURCC_BGR              SGL_FOURCC('B', 'G', 'R', '3')

/** SGL extension; full-resolution planes `R, G, B, A`. */
#define SGL_FOURCC_RGBA_PLANAR      SGL_FOURCC('R', 'A', '4', 'P')
/** SGL extension; full-resolution planes `A, R, G, B`. */
#define SGL_FOURCC_ARGB_PLANAR      SGL_FOURCC('A', 'R', '4', 'P')
/** SGL extension; full-resolution planes `B, G, R, A`. */
#define SGL_FOURCC_BGRA_PLANAR      SGL_FOURCC('B', 'A', '4', 'P')
/** SGL extension; full-resolution planes `A, B, G, R`. */
#define SGL_FOURCC_ABGR_PLANAR      SGL_FOURCC('A', 'B', '4', 'P')
/** SGL extension; full-resolution planes `R, G, B`. */
#define SGL_FOURCC_RGB_PLANAR       SGL_FOURCC('R', 'G', '3', 'P')
/** SGL extension; full-resolution planes `B, G, R`. */
#define SGL_FOURCC_BGR_PLANAR       SGL_FOURCC('B', 'G', '3', 'P')

/** V4L2 YUV24 code; 4:4:4 packed bytes `Y, Cb, Cr`. */
#define SGL_FOURCC_YUV444_PACKED    SGL_FOURCC('Y', 'U', 'V', '3')
/** DRM YUV444 code; 4:4:4 full-resolution planes `Y, Cb, Cr`. */
#define SGL_FOURCC_YUV444_PLANAR    SGL_FOURCC('Y', 'U', '2', '4')

/** DRM/V4L2 YUYV; each two-pixel group is `Y0, Cb0, Y1, Cr0`. */
#define SGL_FOURCC_YUYV             SGL_FOURCC('Y', 'U', 'Y', 'V')
/** DRM/V4L2 YVYU; each two-pixel group is `Y0, Cr0, Y1, Cb0`. */
#define SGL_FOURCC_YVYU             SGL_FOURCC('Y', 'V', 'Y', 'U')
/** DRM/V4L2 UYVY; each two-pixel group is `Cb0, Y0, Cr0, Y1`. */
#define SGL_FOURCC_UYVY             SGL_FOURCC('U', 'Y', 'V', 'Y')
/** DRM/V4L2 VYUY; each two-pixel group is `Cr0, Y0, Cb0, Y1`. */
#define SGL_FOURCC_VYUY             SGL_FOURCC('V', 'Y', 'U', 'Y')
/** DRM YUV422 code; planes `Y, Cb, Cr` with half-width chroma. */
#define SGL_FOURCC_YUV422_PLANAR    SGL_FOURCC('Y', 'U', '1', '6')

/** DRM YUV420 code; planes `Y, Cb, Cr` with half-size chroma axes. */
#define SGL_FOURCC_YUV420_PLANAR    SGL_FOURCC('Y', 'U', '1', '2')
/** DRM/V4L2 NV12; full `Y` plane followed by interleaved `Cb, Cr` 4:2:0. */
#define SGL_FOURCC_NV12             SGL_FOURCC('N', 'V', '1', '2')
/** DRM/V4L2 NV21; full `Y` plane followed by interleaved `Cr, Cb` 4:2:0. */
#define SGL_FOURCC_NV21             SGL_FOURCC('N', 'V', '2', '1')
/** @} */

#endif  /* SGL_FOURCC_H_ */
