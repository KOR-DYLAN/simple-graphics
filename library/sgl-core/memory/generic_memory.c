#include "memory_operation.h"

#if defined(_WIN64)
#define SGL_GENERIC_MEMORY_HAS_64BIT_BULK    (1)
#elif defined(_WIN32)
#define SGL_GENERIC_MEMORY_HAS_64BIT_BULK    (0)
#elif defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ >= 8)
#define SGL_GENERIC_MEMORY_HAS_64BIT_BULK    (1)
#else
#define SGL_GENERIC_MEMORY_HAS_64BIT_BULK    (0)
#endif

#define SGL_GENERIC_MEMORY_BULK_64_SIZE      (8U)
#define SGL_GENERIC_MEMORY_BULK_32_SIZE      (4U)
#define SGL_GENERIC_MEMORY_BULK_16_SIZE      (2U)

void *sgl_generic_memcpy(void *SGL_RESTRICT destination,
                         const void *SGL_RESTRICT source,
                         sgl_size_t size)
{
    sgl_uint8_t *destination_bytes = sgl_memory_as_uint8(destination);
    const sgl_uint8_t *source_bytes = sgl_memory_as_const_uint8(source);
    sgl_size_t offset = 0U;

    /*
     * Bulk copies are deliberately expressed as byte accesses. This keeps
     * unaligned buffers valid and avoids strict-aliasing violations.
     *
     * The initial block size follows the architecture pointer width:
     *
     *   64-bit target: 8 -> 4 -> 2 -> 1
     *   32-bit target:      4 -> 2 -> 1
     */
#if (SGL_GENERIC_MEMORY_HAS_64BIT_BULK == 1)
    if (size >= SGL_GENERIC_MEMORY_BULK_64_SIZE) {
        while ((size - offset) >= SGL_GENERIC_MEMORY_BULK_64_SIZE) {
            destination_bytes[offset] = source_bytes[offset];
            destination_bytes[offset + 1U] = source_bytes[offset + 1U];
            destination_bytes[offset + 2U] = source_bytes[offset + 2U];
            destination_bytes[offset + 3U] = source_bytes[offset + 3U];
            destination_bytes[offset + 4U] = source_bytes[offset + 4U];
            destination_bytes[offset + 5U] = source_bytes[offset + 5U];
            destination_bytes[offset + 6U] = source_bytes[offset + 6U];
            destination_bytes[offset + 7U] = source_bytes[offset + 7U];
            offset += SGL_GENERIC_MEMORY_BULK_64_SIZE;
        }
    }
#endif

    while ((size - offset) >= SGL_GENERIC_MEMORY_BULK_32_SIZE) {
        destination_bytes[offset] = source_bytes[offset];
        destination_bytes[offset + 1U] = source_bytes[offset + 1U];
        destination_bytes[offset + 2U] = source_bytes[offset + 2U];
        destination_bytes[offset + 3U] = source_bytes[offset + 3U];
        offset += SGL_GENERIC_MEMORY_BULK_32_SIZE;
    }

    if ((size - offset) >= SGL_GENERIC_MEMORY_BULK_16_SIZE) {
        destination_bytes[offset] = source_bytes[offset];
        destination_bytes[offset + 1U] = source_bytes[offset + 1U];
        offset += SGL_GENERIC_MEMORY_BULK_16_SIZE;
    }

    if (offset < size) {
        destination_bytes[offset] = source_bytes[offset];
    }

    return destination;
}

void *sgl_generic_memset(void *destination, sgl_int32_t value, sgl_size_t size)
{
    sgl_uint8_t *destination_bytes = sgl_memory_as_uint8(destination);
    sgl_uint8_t byte_value = (sgl_uint8_t)value;
    sgl_size_t offset = 0U;

#if (SGL_GENERIC_MEMORY_HAS_64BIT_BULK == 1)
    if (size >= SGL_GENERIC_MEMORY_BULK_64_SIZE) {
        while ((size - offset) >= SGL_GENERIC_MEMORY_BULK_64_SIZE) {
            destination_bytes[offset] = byte_value;
            destination_bytes[offset + 1U] = byte_value;
            destination_bytes[offset + 2U] = byte_value;
            destination_bytes[offset + 3U] = byte_value;
            destination_bytes[offset + 4U] = byte_value;
            destination_bytes[offset + 5U] = byte_value;
            destination_bytes[offset + 6U] = byte_value;
            destination_bytes[offset + 7U] = byte_value;
            offset += SGL_GENERIC_MEMORY_BULK_64_SIZE;
        }
    }
#endif

    while ((size - offset) >= SGL_GENERIC_MEMORY_BULK_32_SIZE) {
        destination_bytes[offset] = byte_value;
        destination_bytes[offset + 1U] = byte_value;
        destination_bytes[offset + 2U] = byte_value;
        destination_bytes[offset + 3U] = byte_value;
        offset += SGL_GENERIC_MEMORY_BULK_32_SIZE;
    }

    if ((size - offset) >= SGL_GENERIC_MEMORY_BULK_16_SIZE) {
        destination_bytes[offset] = byte_value;
        destination_bytes[offset + 1U] = byte_value;
        offset += SGL_GENERIC_MEMORY_BULK_16_SIZE;
    }

    if (offset < size) {
        destination_bytes[offset] = byte_value;
    }

    return destination;
}
