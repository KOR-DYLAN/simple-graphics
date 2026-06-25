#include "memory_operation.h"

/*
 * Compile-time dispatch keeps the generic implementation available on every
 * target while allowing architecture-specific implementations to replace the
 * byte loop without introducing runtime CPU detection.
 *
 *   caller
 *      |
 *      +-- SGL_CFG_HAS_NEON --> sgl_simd_memcpy / sgl_simd_memset
 *      |
 *      +-- otherwise ---------> sgl_generic_memcpy / sgl_generic_memset
 */
void *sgl_memcpy(void *SGL_RESTRICT destination,
                 const void *SGL_RESTRICT source,
                 sgl_size_t size)
{
    void *result;

#if defined(SGL_CFG_HAS_NEON)
    result = sgl_simd_memcpy(destination, source, size);
#else
    result = sgl_generic_memcpy(destination, source, size);
#endif

    return result;
}

void *sgl_memset(void *destination, sgl_int32_t value, sgl_size_t size)
{
    void *result;

#if defined(SGL_CFG_HAS_NEON)
    result = sgl_simd_memset(destination, value, size);
#else
    result = sgl_generic_memset(destination, value, size);
#endif

    return result;
}
