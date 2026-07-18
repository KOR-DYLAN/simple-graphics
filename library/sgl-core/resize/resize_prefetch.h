/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_RESIZE_PREFETCH_H_
#define SGL_RESIZE_PREFETCH_H_

#include <sgl-core.h>

#if !defined(SGL_RESIZE_PREFETCH_DISTANCE_BYTES)
#define SGL_RESIZE_PREFETCH_DISTANCE_BYTES (128)
#endif

#if !defined(SGL_RESIZE_PREFETCH_INTERVAL)
#define SGL_RESIZE_PREFETCH_INTERVAL       (16)
#endif

#if (SGL_RESIZE_PREFETCH_DISTANCE_BYTES <= 0)
#error "SGL_RESIZE_PREFETCH_DISTANCE_BYTES must be greater than zero"
#endif

#if (SGL_RESIZE_PREFETCH_INTERVAL <= 0)
#error "SGL_RESIZE_PREFETCH_INTERVAL must be greater than zero"
#endif

#if ((SGL_RESIZE_PREFETCH_INTERVAL & (SGL_RESIZE_PREFETCH_INTERVAL - 1)) != 0)
#error "SGL_RESIZE_PREFETCH_INTERVAL must be a power of two"
#endif

#define SGL_RESIZE_PREFETCH_INTERVAL_MASK \
    ((sgl_uint32_t)(SGL_RESIZE_PREFETCH_INTERVAL - 1))
#define SGL_RESIZE_PREFETCH_IS_DUE(iteration) \
    ((((sgl_uint32_t)(iteration)) & SGL_RESIZE_PREFETCH_INTERVAL_MASK) == 0U)

/*
 * Resize source prefetch
 * ----------------------
 * Indexed resize reads are monotonic but skip source pixels during downscale.
 * Issue one hint per interval and keep the hinted address inside the row:
 *
 *   current source byte ---- byte distance ----> prefetched cache line
 *            |                                      |
 *            +------- every N destination columns --+
 *
 * The distance and interval are compile-time overrides so each target can tune
 * them without changing the compiler abstraction or the resize kernels.
 */
static SGL_ALWAYS_INLINE void sgl_resize_prefetch_source_read(
    const sgl_uint8_t *row,
    sgl_int32_t byte_offset,
    sgl_int32_t row_size,
    sgl_int32_t iteration)
{
    if (SGL_UNLIKELY(
            SGL_RESIZE_PREFETCH_IS_DUE(iteration) &&
            (row_size >= SGL_RESIZE_PREFETCH_DISTANCE_BYTES) &&
            (byte_offset <=
             (row_size - SGL_RESIZE_PREFETCH_DISTANCE_BYTES))))
    {
        SGL_PREFETCH_READ(
            &row[byte_offset], SGL_RESIZE_PREFETCH_DISTANCE_BYTES);
    }
}

#endif  /* SGL_RESIZE_PREFETCH_H_ */
