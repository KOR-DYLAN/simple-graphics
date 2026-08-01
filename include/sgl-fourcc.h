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

#include <sgl-type.h>

typedef sgl_uint32_t                sgl_fourcc_t;

/*
 * DRM/V4L2 FourCC layout: a occupies bits 7:0, b bits 15:8,
 * c bits 23:16, and d bits 31:24.
 */
#define SGL_FOURCC(a, b, c, d) \
    ((sgl_fourcc_t)((sgl_uint8_t)(a)) | \
     ((sgl_fourcc_t)((sgl_uint8_t)(b)) << 8U) | \
     ((sgl_fourcc_t)((sgl_uint8_t)(c)) << 16U) | \
     ((sgl_fourcc_t)((sgl_uint8_t)(d)) << 24U))

#define SGL_FOURCC_RGBA             SGL_FOURCC('A', 'B', '2', '4')  /* V4L2 RGBA32 / DRM ABGR8888 code; packed bytes R, G, B, A. */
#define SGL_FOURCC_ARGB             SGL_FOURCC('B', 'A', '2', '4')  /* V4L2 ARGB32 / DRM BGRA8888 code; packed bytes A, R, G, B. */
#define SGL_FOURCC_BGRA             SGL_FOURCC('A', 'R', '2', '4')  /* V4L2 ABGR32 / DRM ARGB8888 code; packed bytes B, G, R, A. */
#define SGL_FOURCC_ABGR             SGL_FOURCC('R', 'A', '2', '4')  /* V4L2 BGRA32 / DRM RGBA8888 code; packed bytes A, B, G, R. */
#define SGL_FOURCC_RGB              SGL_FOURCC('R', 'G', 'B', '3')  /* V4L2 RGB24 code; packed bytes R, G, B. */
#define SGL_FOURCC_BGR              SGL_FOURCC('B', 'G', 'R', '3')  /* V4L2 BGR24 code; packed bytes B, G, R. */

/* SGL extensions: DRM/V4L2 do not define RGB-only planar FourCC values. */
#define SGL_FOURCC_RGBA_PLANAR      SGL_FOURCC('R', 'A', '4', 'P')  /* SGL RGBA planar: planes R, G, B, A. */
#define SGL_FOURCC_ARGB_PLANAR      SGL_FOURCC('A', 'R', '4', 'P')  /* SGL ARGB planar: planes A, R, G, B. */
#define SGL_FOURCC_BGRA_PLANAR      SGL_FOURCC('B', 'A', '4', 'P')  /* SGL BGRA planar: planes B, G, R, A. */
#define SGL_FOURCC_ABGR_PLANAR      SGL_FOURCC('A', 'B', '4', 'P')  /* SGL ABGR planar: planes A, B, G, R. */
#define SGL_FOURCC_RGB_PLANAR       SGL_FOURCC('R', 'G', '3', 'P')  /* SGL RGB planar: planes R, G, B. */
#define SGL_FOURCC_BGR_PLANAR       SGL_FOURCC('B', 'G', '3', 'P')  /* SGL BGR planar: planes B, G, R. */

#define SGL_FOURCC_YUV444_PACKED    SGL_FOURCC('Y', 'U', 'V', '3')  /* V4L2 YUV24 code; SGL packed bytes Y, Cb, Cr. */
#define SGL_FOURCC_YUV444_PLANAR    SGL_FOURCC('Y', 'U', '2', '4')  /* DRM YUV444 code; SGL planes Y, Cb, Cr; 4:4:4. */

#define SGL_FOURCC_YUYV             SGL_FOURCC('Y', 'U', 'Y', 'V')  /* DRM/V4L2 YUYV: 4 bytes for 2 pixels: Y0, Cb0, Y1, Cr0. */
#define SGL_FOURCC_YVYU             SGL_FOURCC('Y', 'V', 'Y', 'U')  /* DRM/V4L2 YVYU: 4 bytes for 2 pixels: Y0, Cr0, Y1, Cb0. */
#define SGL_FOURCC_UYVY             SGL_FOURCC('U', 'Y', 'V', 'Y')  /* DRM/V4L2 UYVY: 4 bytes for 2 pixels: Cb0, Y0, Cr0, Y1. */
#define SGL_FOURCC_VYUY             SGL_FOURCC('V', 'Y', 'U', 'Y')  /* DRM/V4L2 VYUY: 4 bytes for 2 pixels: Cr0, Y0, Cb0, Y1. */
#define SGL_FOURCC_YUV422_PLANAR    SGL_FOURCC('Y', 'U', '1', '6')  /* DRM YUV422 code; SGL planes Y, Cb, Cr; 4:2:2. */

#define SGL_FOURCC_YUV420_PLANAR    SGL_FOURCC('Y', 'U', '1', '2')  /* DRM YUV420 code; SGL planes Y, Cb, Cr; 4:2:0. */
#define SGL_FOURCC_NV12             SGL_FOURCC('N', 'V', '1', '2')  /* DRM/V4L2 NV12: plane Y, then interleaved Cb, Cr; 4:2:0. */
#define SGL_FOURCC_NV21             SGL_FOURCC('N', 'V', '2', '1')  /* DRM/V4L2 NV21: plane Y, then interleaved Cr, Cb; 4:2:0. */

#endif  /* SGL_FOURCC_H_ */
