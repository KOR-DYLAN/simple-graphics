/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_RESIZE_BITOPS_H_
#define SGL_RESIZE_BITOPS_H_

#include <sgl-core.h>

#define SGL_RESIZE_BPP16_BYTE_SHIFT (1U)
#define SGL_RESIZE_BPP32_BYTE_SHIFT (2U)

/*
 * Fixed pixel-to-byte offsets
 * ---------------------------
 * Resize dimensions and LUT coordinates are validated as nonnegative before
 * entering a kernel.  Express fixed 2/4-byte pixel strides as shifts while
 * keeping the byte-offset meaning visible at each call site:
 *
 *   pixel index -- << 1 --> BPP16 byte offset
 *   pixel index -- << 2 --> BPP32 byte offset
 *
 * BPP24 intentionally remains a multiplication because three is not a power
 * of two.
 */
#define SGL_RESIZE_BPP16_BYTE_OFFSET(pixel_index) \
    ((sgl_uint32_t)(pixel_index) << SGL_RESIZE_BPP16_BYTE_SHIFT)
#define SGL_RESIZE_BPP32_BYTE_OFFSET(pixel_index) \
    ((sgl_uint32_t)(pixel_index) << SGL_RESIZE_BPP32_BYTE_SHIFT)

#endif  /* SGL_RESIZE_BITOPS_H_ */
