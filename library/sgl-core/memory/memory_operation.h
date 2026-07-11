#ifndef SGL_MEMORY_OPERATION_H_
#define SGL_MEMORY_OPERATION_H_

#include <sgl-core.h>
#include <sgl_memory_cast.h>

void *sgl_generic_memcpy(void *SGL_RESTRICT destination,
                         const void *SGL_RESTRICT source,
                         sgl_size_t size);
void *sgl_generic_memset(void *destination,
                         sgl_int32_t value,
                         sgl_size_t size);

#if defined(SGL_CFG_HAS_NEON)
void *sgl_simd_memcpy(void *SGL_RESTRICT destination,
                      const void *SGL_RESTRICT source,
                      sgl_size_t size);
void *sgl_simd_memset(void *destination,
                      sgl_int32_t value,
                      sgl_size_t size);
#endif

#endif /* SGL_MEMORY_OPERATION_H_ */
