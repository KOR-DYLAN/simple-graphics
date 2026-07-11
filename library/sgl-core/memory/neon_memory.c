/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <arm_neon.h>
#include "memory_operation.h"

#define SGL_NEON_VECTOR_SIZE       (16U)
#define SGL_NEON_BULK_SIZE         (64U)

void *sgl_simd_memcpy(void *SGL_RESTRICT destination,
                      const void *SGL_RESTRICT source,
                      sgl_size_t size)
{
    sgl_uint8_t *destination_bytes = sgl_memory_as_uint8(destination);
    const sgl_uint8_t *source_bytes = sgl_memory_as_const_uint8(source);
    sgl_size_t offset = 0U;

    /*
     * Select the widest useful path from the requested size:
     *
     *   64-byte bulk blocks -> 16-byte vectors -> generic byte tail
     */
    if (size >= SGL_NEON_BULK_SIZE) {
        while ((size - offset) >= SGL_NEON_BULK_SIZE) {
            uint8x16_t vector0 = vld1q_u8(&source_bytes[offset]);
            uint8x16_t vector1 = vld1q_u8(&source_bytes[offset + 16U]);
            uint8x16_t vector2 = vld1q_u8(&source_bytes[offset + 32U]);
            uint8x16_t vector3 = vld1q_u8(&source_bytes[offset + 48U]);
            vst1q_u8(&destination_bytes[offset], vector0);
            vst1q_u8(&destination_bytes[offset + 16U], vector1);
            vst1q_u8(&destination_bytes[offset + 32U], vector2);
            vst1q_u8(&destination_bytes[offset + 48U], vector3);
            offset += SGL_NEON_BULK_SIZE;
        }
    }

    if ((size - offset) >= SGL_NEON_VECTOR_SIZE) {
        while ((size - offset) >= SGL_NEON_VECTOR_SIZE) {
            uint8x16_t vector0 = vld1q_u8(&source_bytes[offset]);
            vst1q_u8(&destination_bytes[offset], vector0);
            offset += SGL_NEON_VECTOR_SIZE;
        }
    }

    if (offset < size) {
        (void)sgl_generic_memcpy(&destination_bytes[offset],
                                 &source_bytes[offset],
                                 size - offset);
    }

    return destination;
}

void *sgl_simd_memset(void *destination, sgl_int32_t value, sgl_size_t size)
{
    sgl_uint8_t *destination_bytes = sgl_memory_as_uint8(destination);
    sgl_uint8_t byte_value = (sgl_uint8_t)value;
    sgl_size_t offset = 0U;
    uint8x16_t vector = vdupq_n_u8(byte_value);

    if (size >= SGL_NEON_BULK_SIZE) {
        while ((size - offset) >= SGL_NEON_BULK_SIZE) {
            vst1q_u8(&destination_bytes[offset], vector);
            vst1q_u8(&destination_bytes[offset + 16U], vector);
            vst1q_u8(&destination_bytes[offset + 32U], vector);
            vst1q_u8(&destination_bytes[offset + 48U], vector);
            offset += SGL_NEON_BULK_SIZE;
        }
    }

    if ((size - offset) >= SGL_NEON_VECTOR_SIZE) {
        while ((size - offset) >= SGL_NEON_VECTOR_SIZE) {
            vst1q_u8(&destination_bytes[offset], vector);
            offset += SGL_NEON_VECTOR_SIZE;
        }
    }

    if (offset < size) {
        (void)sgl_generic_memset(&destination_bytes[offset],
                                 value,
                                 size - offset);
    }

    return destination;
}
